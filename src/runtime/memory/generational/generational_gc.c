#ifdef USING_GEN_GC_ALG

#include "generational_gc.h"

extern __thread struct vm_thread * self_thread;
extern struct config config;
extern struct vm current_vm;
extern void start_minor_generational_gc(bool start_major);
extern void write_struct_field_barrier_generational_gc(struct struct_instance_object *, struct object *);
extern void write_array_element_barrier_generational_gc(struct array_object *, struct object *);
extern void on_gc_finished_vm(struct gc_result result);

static struct object * try_alloc_object(size_t size);
static void try_claim_eden_block_or_start_gc(size_t size_bytes);
static void save_new_eden_block_info(struct eden_block_allocation);
static void notify_start_gc_signal_acked();
static void await_until_gc_finished();

static void * generational_gc_lox_malloc(struct lox_allocator *, size_t);
static void generational_gc_lox_free(struct lox_allocator *, void *);

struct gc_barriers get_barriers_gc_alg() {
    return (struct gc_barriers) {
        .write_array_element = write_array_element_barrier_generational_gc,
        .write_struct_field = write_struct_field_barrier_generational_gc
    };
}

void check_gc_on_safe_point_alg() {
    struct generational_gc * gc = current_vm.gc;
    gc_gen_state_t current_gc_state = atomic_load(&gc->state);

    switch (current_gc_state) {
        case GC_NONE: return;
        case GC_WAITING: {
            //There is a race jump_to_operand if a thread terminates and the gc starts.
            //The gc started thread might have read a stale value_node of the number_current_threads, and if other thread
            //has termianted, the gc thread will block forever
            //To solve this we want to wake up the gc thread to revaluate its waiting jump_to_operand.
            //This will work because, calling this method, number_current_threads has been decreased (see runtime code)
            atomic_fetch_add(&gc->number_threads_ack_start_gc_signal, 1);
            notify_start_gc_signal_acked();

            if(self_thread->state != THREAD_TERMINATED) {
                await_until_gc_finished();
            }
        }
            //If this gets run this means we have been sleeping
        case GC_IN_PROGRESS:
            await_until_gc_finished();
            break;
    }
}

struct struct_instance_object * alloc_struct_instance_gc_alg(struct struct_definition_object * definition) {
    struct generational_gc * gc = current_vm.gc;

    int n_fields_hashtable_entries = get_req_capacity_lox_hash_table(definition->n_fields);
    size_t struct_instance_size = sizeof(struct struct_instance_object) + (sizeof(struct hash_table_entry) * n_fields_hashtable_entries);
    size_t struct_instance_size_aligned = align(struct_instance_size, sizeof(struct object));
    struct struct_instance_object * instance = (struct struct_instance_object *) try_alloc_object(struct_instance_size_aligned);

    init_struct_instance_object(instance, definition);
    //Struct fields hashtable is not expected to resize, since the number of fields in a struct is always the same in runtime
    init_rw_mutex(&instance->fields.rw_lock);
    instance->fields.capacity = n_fields_hashtable_entries;
    instance->fields.allocator = NULL;
    instance->fields.size = 0;
    struct hash_table_entry * start_fields_ptr = (struct hash_table_entry *) (((uint8_t *) instance) + sizeof(struct struct_instance_object));
    instance->fields.entries = start_fields_ptr;
    for(int i = 0; i < n_fields_hashtable_entries; i++){
        start_fields_ptr[i].key = NULL;
        start_fields_ptr[i].value = 0;
    }

    return instance;
}

struct string_object * alloc_string_gc_alg(char * chars, int length) {
    size_t aligned_total_size = align(sizeof(struct string_object) + length + 1, sizeof(struct object));
    struct string_object * string_ptr = (struct string_object *) try_alloc_object(aligned_total_size);
    init_object(&string_ptr->object, OBJ_STRING);
    string_ptr->length = length;
    string_ptr->hash = hash_string(chars, length);
    string_ptr->chars = (char *) (((uint8_t *) string_ptr) + sizeof(struct string_object));
    memcpy(string_ptr->chars, chars, length);
    string_ptr->chars[length] = '\0';
    NATIVE_LOX_FREE(chars);

    return string_ptr;
}

struct array_object * alloc_array_gc_alg(int n_elements) {
    struct generational_thread_gc * gc_thread_info = self_thread->gc_info;

    size_t not_aligned_size = sizeof(struct array_object) + (n_elements * sizeof(lox_value_t));
    size_t total_size_aligned = align(not_aligned_size, sizeof(struct object));
    struct array_object * array = (struct array_object *) try_alloc_object(total_size_aligned);

    init_object(&array->object, OBJ_ARRAY);
    array->values.values = (lox_value_t *) (((uint8_t *) array) + sizeof(struct array_object));
    array->values.capacity = n_elements;
    array->values.in_use = n_elements;
    array->values.allocator = NULL; //The array won't grow/shrink

    return array;
}

void * alloc_gc_object_info_alg() {
    return NULL;
}

void * alloc_gc_thread_info_alg() {
    struct generational_thread_gc * generational_gc = NATIVE_LOX_MALLOC(sizeof(struct generational_thread_gc));
    generational_gc->eden = alloc_eden_thread();
    return generational_gc;
}

void * alloc_gc_vm_info_alg() {
    struct generational_gc * generational_gc = NATIVE_LOX_MALLOC(sizeof(struct generational_gc));
    generational_gc->survivor = alloc_survivor(config);
    generational_gc->eden = alloc_eden(config);
    generational_gc->old = alloc_old(config);
    generational_gc->previous_major = false;
    generational_gc->state = GC_NONE;
    pthread_cond_init(&generational_gc->await_ack_start_gc_signal_cond, NULL);
    init_mutex(&generational_gc->await_ack_start_gc_signal_mutex);
    pthread_cond_init(&generational_gc->await_gc_cond, NULL);
    init_mutex(&generational_gc->await_gc_cond_mutex);
    generational_gc->number_threads_ack_start_gc_signal = 0;
    generational_gc->allocator = (struct generational_gc_lox_allocator) {
        .lox_allocator = (struct lox_allocator) {
            .lox_malloc = generational_gc_lox_malloc,
            .lox_free = generational_gc_lox_free
        }
    };

    return generational_gc;
}

struct gc_result try_start_gc_alg(lox_value_t * args) {
    struct generational_gc * gc = current_vm.gc;
    gc_gen_state_t expected = GC_NONE;
    bool start_major = args != NULL && args[0] == TRUE_VALUE;

    if (atomic_compare_exchange_strong(&gc->state, &expected, GC_WAITING)) {
        size_t bytes_allocated_before_gc = get_bytes_allocated_generational_gc(gc);
        start_minor_generational_gc(start_major);
        size_t bytes_allocated_after_gc = get_bytes_allocated_generational_gc(gc);

        atomic_store_explicit(&gc->state, GC_NONE, memory_order_release);

        struct gc_result result = (struct gc_result) {
                .bytes_allocated_before_gc = bytes_allocated_before_gc,
                .bytes_allocated_after_gc = bytes_allocated_after_gc
        };

        on_gc_finished_vm(result);

        return result;
    } else {
        check_gc_on_safe_point_alg();
        return (struct gc_result) {
                .bytes_allocated_before_gc = 0,
                .bytes_allocated_after_gc = 0,
        };
    }
}

static struct object * try_alloc_object(size_t size_bytes) {
    struct generational_thread_gc * gc_thread_info = self_thread->gc_info;
    struct eden_thread * eden_thread_info = gc_thread_info->eden;

    if (!can_allocate_object_in_block_eden(eden_thread_info, size_bytes)) {
        try_claim_eden_block_or_start_gc(size_bytes);
    }

    return allocate_object_in_block_eden(eden_thread_info, size_bytes);
}

static void try_claim_eden_block_or_start_gc(size_t size_bytes) {
    struct generational_gc * global_gc_info = current_vm.gc;
    int n_blocks = (int) ceil(size_bytes / ((double) config.generational_gc_config.eden_block_size_kb * 1024));
    struct eden_block_allocation block_allocation = try_claim_eden_block(global_gc_info->eden, n_blocks);

    if (!block_allocation.success) {
        try_start_gc_alg(NULL);
        block_allocation = try_claim_eden_block(global_gc_info->eden, n_blocks);
    }

    save_new_eden_block_info(block_allocation);
}

static void save_new_eden_block_info(struct eden_block_allocation eden_block_allocation) {
    struct generational_thread_gc * gc_thread_info = self_thread->gc_info;
    struct eden_thread * eden_thread_info = gc_thread_info->eden;

    eden_thread_info->start_block = eden_block_allocation.start_block;
    eden_thread_info->current_block = eden_block_allocation.start_block;
    eden_thread_info->end_block = eden_block_allocation.end_block;
}

bool belongs_to_young_generational_gc(struct generational_gc * gc, uintptr_t ptr) {
    return belongs_to_eden(gc->eden, ptr) || belongs_to_survivor(gc->survivor, ptr);
}

struct card_table * get_card_table_generational_gc(struct generational_gc * gc, uintptr_t ptr) {
    if (belongs_to_survivor(gc->survivor, ptr)) {
        return &gc->survivor->fromspace_card_table;
    } else if (belongs_to_eden(gc->eden, ptr)) {
        return &gc->eden->card_table;
    }

    return NULL;
}

bool belongs_to_heap_generational_gc(struct generational_gc * gc, uintptr_t ptr) {
    return belongs_to_eden(gc->eden, ptr) ||
            belongs_to_survivor(gc->survivor, ptr) ||
            belongs_to_old(gc->old, ptr);
}

void clear_mark_bitmaps_generational_gc(struct generational_gc * generational_gc) {
    reset_mark_bitmap(generational_gc->eden->mark_bitmap);
    reset_mark_bitmap(&generational_gc->survivor->fromspace_mark_bitmap);
    reset_mark_bitmap(generational_gc->old->mark_bitmap);
}

struct mark_bitmap * get_mark_bitmap_generational_gc(struct generational_gc * gc, uintptr_t ptr) {
    if(belongs_to_eden(gc->eden, ptr)){
        return gc->eden->mark_bitmap;
    } else if (belongs_to_survivor(gc->survivor, ptr)) {
        return &gc->survivor->fromspace_mark_bitmap;
    } else if (belongs_to_old(gc->old, ptr)) {
        return gc->old->mark_bitmap;
    } else {
        return NULL;
    }
}

void clear_card_tables_generational_gc(struct generational_gc * gc) {
    struct generational_gc * generational_gc = current_vm.gc;
    clear_card_table(&generational_gc->survivor->fromspace_card_table);
    clear_card_table(&generational_gc->eden->card_table);
}

bool is_marked_generational_gc(struct generational_gc * gc, uintptr_t ptr) {
    if (belongs_to_eden(gc->eden, ptr)) {
        return is_marked_bitmap(gc->eden->mark_bitmap, (void *) ptr);
    } else if (belongs_to_survivor(gc->survivor, ptr)) {
        return is_marked_bitmap(&gc->survivor->fromspace_mark_bitmap, (void *) ptr);
    } else if (belongs_to_old(gc->old, ptr)) {
        return is_marked_bitmap(gc->old->mark_bitmap, (void *) ptr);
    } else {
        return false;
    }
}

static void await_until_gc_finished() {
    struct generational_gc * gc = current_vm.gc;

    lock_mutex(&gc->await_gc_cond_mutex);

    while(gc->state != GC_NONE) {
        pthread_cond_wait(&gc->await_gc_cond, &gc->await_gc_cond_mutex.native_mutex);
    }

    unlock_mutex(&gc->await_gc_cond_mutex);
}

static void notify_start_gc_signal_acked() {
    struct generational_gc * gc = current_vm.gc;

    lock_mutex(&gc->await_ack_start_gc_signal_mutex);

    pthread_cond_signal(&gc->await_ack_start_gc_signal_cond);

    unlock_mutex(&gc->await_ack_start_gc_signal_mutex);
}

size_t get_bytes_allocated_generational_gc(struct generational_gc * gc) {
    size_t allocated_eden = get_allocated_bytes_memory_space(&gc->eden->memory_space);
    size_t allocated_survivor = get_allocated_bytes_memory_space(gc->survivor->from);
    size_t allocated_old = get_allocated_bytes_memory_space(&gc->old->memory_space);

    return allocated_eden + allocated_survivor + allocated_old;
}

static void * generational_gc_lox_malloc(struct lox_allocator * lox_allocator, size_t size) {
    return (struct object *) try_alloc_object(size);
}

static void generational_gc_lox_free(struct lox_allocator*, void *) {
    //We don't do anything, the memory will get reclaimed in a minir/major gc pause.
}

#endif