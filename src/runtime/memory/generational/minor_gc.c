#include "runtime/memory/generational/generational_gc.h"
#include "runtime/memory/generational/utils.h"

#include "runtime/threads/vm_thread.h"
#include "runtime/vm.h"

#include "shared/config/config.h"

extern struct vm current_vm;
extern struct trie_list * compiled_packages;
extern struct config config;
extern struct string_pool global_string_pool;

extern void runtime_panic(char * format, ...);
extern void start_major_generational_gc();

struct object_to_traverse {
    lox_value_t * reference_holder;
    struct object * object;
};

struct for_each_thread_traverse_and_move {
    struct stack_list * pending;
    bool * moved_all_successfuly;
};

static struct object_to_traverse * alloc_object_to_traverse(struct object * object, lox_value_t * reference_holder);
static bool traverse_thread_stack_to_move(struct vm_thread *, struct vm_thread *, int index, void *);
static bool traverse_heap_and_move(struct stack_list * terminated_threads);
static bool traverse_thread_stack_to_update_references(struct vm_thread *, struct vm_thread *, int index, void *);
static bool traverse_package_globals_to_move(void *, void *);
static void traverse_value_and_update_references(lox_value_t root_value, lox_value_t * root_reference_holder);
static bool traverse_value_and_move(lox_value_t root_value);
static bool can_be_moved(struct object * object);
static void mark_object(struct object * object);
static bool move_object(struct object * object);
static void traverse_lox_arraylist(struct lox_arraylist array_list, struct stack_list * pending);
static void traverse_lox_hashtable(struct lox_hash_table hash_table, struct stack_list * pending);
static uint8_t * move_to_new_space(struct object *);
static void update_references();
static void remove_terminated_threads(struct stack_list * terminated_threads);
static bool has_been_updated(struct object *);
static void mark_as_updated(struct object *);
static bool traverse_package_globals_to_update_references(void * trie_node_ptr, void * extra_ignored);
static bool for_each_card_table_entry_function(void * card_table_dirty_address, void * extra);
static bool traverse_object_and_move(struct object * root_object);
static void update_card_tables();
static void mark_references_in_card_table(struct object * object_root_in_old);
static void traverse_struct_instance_to_update_card_table(struct struct_instance_object *, struct stack_list *);
static void traverse_array_to_update_card_table(struct array_object *, struct stack_list *);
static void signal_threads_start_gc_alg_and_await();
static void await_all_threads_signal_start_gc();
static void notify_start_gc_signal_acked();
static void await_until_gc_finished();
static void signal_threads_gc_finished_alg();
static void reset_eden_threads_data();
static bool reset_eden_thread_data(struct vm_thread *, struct vm_thread *, int, void *);
static void update_string_pool_references();

void start_minor_generational_gc(bool start_major) {
    struct generational_gc * gc = current_vm.gc;
    struct stack_list terminated_threads;
    init_stack_list(&terminated_threads, NATIVE_LOX_ALLOCATOR());

    signal_threads_start_gc_alg_and_await();

    //Start major gc and restart minor gc
    if (!traverse_heap_and_move(&terminated_threads) || start_major) {
        if (gc->previous_major) {
            runtime_panic("Out of memory!");
        }

        start_major_generational_gc();
        free_stack_list(&terminated_threads);
        start_minor_generational_gc(false);
        return;
    }

    clear_mark_bitmaps_generational_gc(gc);
    update_references();
    clear_card_tables_generational_gc(gc);
    swap_from_to_survivor_space(gc->survivor, config);
    update_card_tables();
    clear_mark_bitmaps_generational_gc(gc);
    remove_terminated_threads(&terminated_threads);
    reset_eden_threads_data();
    reset_memory_space(&gc->eden->memory_space);

    free_stack_list(&terminated_threads);
    gc->previous_major = false;

    atomic_thread_fence(memory_order_release);

    signal_threads_gc_finished_alg();
}

static void reset_eden_threads_data() {
    for_each_thread(current_vm.root, reset_eden_thread_data, NULL,
                    THREADS_OPT_ONLY_ALIVE |
                    THREADS_OPT_RECURSIVE |
                    THREADS_OPT_INCLUSIVE);
}

static bool reset_eden_thread_data(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    struct generational_thread_gc * gc_thread_info = child->gc_info;
    gc_thread_info->eden->current_block = NULL;
    gc_thread_info->eden->start_block = NULL;
    gc_thread_info->eden->end_block = NULL;
    return true;
}

static void update_card_tables() {
    struct generational_gc * gc = current_vm.gc;
    struct old * old = gc->old;

    for(uint8_t * current_ptr = old->memory_space.start; current_ptr < old->memory_space.current;) {
        struct object * current_object = (struct object *) current_ptr;
        mark_references_in_card_table(current_object);
        current_ptr += get_n_bytes_allocated_object(current_object);
    }
}

static void remove_terminated_threads(struct stack_list * terminated_threads) {
    while(!is_empty_stack_list(terminated_threads)){
        struct vm_thread * terminated_thread = pop_stack_list(terminated_threads);

        free_vm_thread(terminated_thread);
        terminated_thread->terminated_state = THREAD_TERMINATED_GC_DONE;
    }
}

static void update_references() {
    //Thread stacks
    for_each_thread(current_vm.root, traverse_thread_stack_to_update_references, NULL,
                    THREADS_OPT_ONLY_ALIVE |
                    THREADS_OPT_RECURSIVE  |
                    THREADS_OPT_INCLUSIVE);

    //Globals
    for_each_node(compiled_packages, NULL, traverse_package_globals_to_update_references);

    update_string_pool_references();
}

static void for_each_string_pool_entry(
        struct string_object * key,
        struct string_object ** key_reference_holder,
        lox_value_t value,
        lox_value_t * value_reference_holder,
        void * extra
) {
    struct string_object * string = (struct string_object *) AS_OBJECT(value);
    struct generational_gc * gc = current_vm.gc;
    lox_value_t forwading_ptr = GET_FORWARDING_PTR((&string->object));
    bool belongs_to_young_heap = belongs_to_young_generational_gc(gc, (uintptr_t) string);
    bool belongs_to_heap = belongs_to_heap_generational_gc(gc, (uintptr_t) string);
    bool has_been_moved = !IS_CLEARED_FORWARDING_PTR(&string->object);

    if (belongs_to_heap && has_been_moved) {
        *key_reference_holder = (struct string_object *) AS_OBJECT(forwading_ptr);
        *value_reference_holder = forwading_ptr;
    } else if (belongs_to_young_heap && !has_been_moved) {
        //Objects in young heap are always moved in every gc. If they haven't been moved, the chars is unreachable,
        //so we need to reclaim its inner_hash_table entry
        remove_hash_table(&global_string_pool.strings, string);
    }
}

static void update_string_pool_references() {
    for_each_value_hash_table(&global_string_pool.strings, NULL, for_each_string_pool_entry);
}

static bool traverse_package_globals_to_update_references(void * trie_node_ptr, void * extra_ignored) {
    struct package * package = (struct package *) ((struct trie_node *) trie_node_ptr)->data;

    for(int i = 0; i < package->global_variables.capacity && package->state != PENDING_COMPILATION; i++) {
        struct hash_table_entry * entry = &package->global_variables.entries[i];
        if (entry != NULL && entry->key != NULL) {
            traverse_value_and_update_references(TO_LOX_VALUE_OBJECT(entry->key), (lox_value_t *) &entry->key);

            if (IS_OBJECT(entry->value)) {
                traverse_value_and_update_references(entry->value, &entry->value);
            }
        }
    }

    return true;
}

static bool traverse_thread_stack_to_update_references(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    for(lox_value_t * value = child->stack; value < child->esp; value++) {
        if (IS_OBJECT(*value)) {
            traverse_value_and_update_references(*value, value);
        }
    }

    return true;
}

static void mark_references_in_card_table(struct object * object_root_in_old) {
    struct generational_gc * gc = current_vm.gc;
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, object_root_in_old);

    while (!is_empty_stack_list(&pending)) {
        struct object * current_object = pop_stack_list(&pending);

        switch (current_object->type) {
            case OBJ_ARRAY:
                traverse_array_to_update_card_table((struct array_object *) current_object, &pending);
                break;
            case OBJ_STRUCT_INSTANCE:
                traverse_struct_instance_to_update_card_table((struct struct_instance_object *) current_object, &pending);
                break;
        }
    }
}

static void traverse_struct_instance_entry_to_update_card_table(
        struct string_object * key,
        struct string_object ** key_reference_holder,
        lox_value_t value,
        lox_value_t * reference_holder,
        void * extra
) {
    struct generational_gc * gc = current_vm.gc;
    struct stack_list * pending = extra;
    struct object * object = AS_OBJECT(value);

    if (IS_OBJECT(value) && belongs_to_heap_generational_gc(gc, (uintptr_t) object)) {
        if (belongs_to_old(gc->old, (uintptr_t) object)) {
            push_stack_list(pending, object);
        } else {
            struct card_table * card = get_card_table_generational_gc(gc, (uintptr_t) object);
            mark_dirty_card_table(card, (uint64_t *) object);
        }
    }
}

static void traverse_struct_instance_to_update_card_table(struct struct_instance_object * instance, struct stack_list * pending) {
    struct generational_gc * gc = current_vm.gc;
    for_each_value_hash_table(&instance->fields, pending, traverse_struct_instance_entry_to_update_card_table);
}

static void traverse_array_to_update_card_table(struct array_object * array, struct stack_list * pending) {
    struct generational_gc * gc = current_vm.gc;

    for (int i = 0; i < array->values.in_use; i++) {
        lox_value_t current_array_value = array->values.values[i];

        if (IS_OBJECT(current_array_value)) {
            struct object * current_array_object = AS_OBJECT(current_array_value);

            if (belongs_to_old(gc->old, (uintptr_t) current_array_object)) {
                push_stack_list(pending, current_array_object);
            } else {
                struct card_table * card = get_card_table_generational_gc(gc, (uintptr_t) current_array_object);
                mark_dirty_card_table(card, (uint64_t *) current_array_object);
            }
        }
    }
}

static void traverse_value_and_update_references(lox_value_t root_value, lox_value_t * root_reference_holder) {
    struct generational_gc * generational_gc = current_vm.gc;
    struct object * root_object = AS_OBJECT(root_value);
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, alloc_object_to_traverse(root_object, root_reference_holder));

    while (!is_empty_stack_list(&pending)) {
        struct object_to_traverse * to_traverse = pop_stack_list(&pending);
        lox_value_t * current_reference_holder = to_traverse->reference_holder;
        struct object * current = to_traverse->object;
        free(to_traverse);
        bool belongs_to_heap = belongs_to_heap_generational_gc(generational_gc, (uintptr_t) current);

        if (belongs_to_heap && !has_been_updated(current)) {
            mark_as_updated(current);

            if (!IS_CLEARED_FORWARDING_PTR(current)) {
                *current_reference_holder = GET_FORWARDING_PTR(current);
                CLEAR_FORWARDING_PTR(current);
            }

            switch (current->type) {
                case OBJ_STRUCT_INSTANCE:
                    traverse_lox_hashtable(((struct struct_instance_object *) current)->fields, &pending);
                    break;
                case OBJ_ARRAY:
                    traverse_lox_arraylist(((struct array_object *) current)->values, &pending);
                    break;
            }
        }
    }
}

//Expect object to be in the heap
static bool has_been_updated(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;
    struct mark_bitmap * mark_bitmap = get_mark_bitmap_generational_gc(generational_gc, object_ptr);
    return is_marked_bitmap(mark_bitmap, (uint64_t *) object);
}

static void mark_as_updated(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;
    struct mark_bitmap * mark_bitmap = get_mark_bitmap_generational_gc(generational_gc, object_ptr);
    set_marked_bitmap(mark_bitmap, (uint64_t *) object);
}

static bool traverse_heap_and_move(struct stack_list * terminated_threads) {
    bool moved_all_successfuly = true;

    //Thread stacks
    struct for_each_thread_traverse_and_move for_each_thread_data = (struct for_each_thread_traverse_and_move) {
            .pending = terminated_threads,
            .moved_all_successfuly = &moved_all_successfuly
    };
    for_each_thread(current_vm.root, traverse_thread_stack_to_move, &for_each_thread_data,
                    THREADS_OPT_INCLUDE_TERMINATED |
                    THREADS_OPT_RECURSIVE |
                    THREADS_OPT_INCLUSIVE);
    //Globals
    for_each_node(compiled_packages, &moved_all_successfuly, traverse_package_globals_to_move);

    //Card tables
    struct generational_gc * generational_gc = current_vm.gc;
    for_each_card_table(&generational_gc->survivor->fromspace_card_table, &moved_all_successfuly, for_each_card_table_entry_function);
    for_each_card_table(&generational_gc->eden->card_table, &moved_all_successfuly, for_each_card_table_entry_function);

    return moved_all_successfuly;
}

static bool for_each_card_table_entry_function(void * card_table_dirty_address, void * extra) {
    bool moved_successfuly = traverse_object_and_move(card_table_dirty_address);
    *((bool *) extra) = moved_successfuly;
    return moved_successfuly;
}

static bool traverse_thread_stack_to_move(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    struct for_each_thread_traverse_and_move * data = terminated_threads_ptr;

    if (child->state == THREAD_TERMINATED && child->terminated_state == THREAD_TERMINATED_PENDING_GC) {
        struct stack_list * terminated_threads = data->pending;
        push_stack_list(terminated_threads, child);
    } else {
        for(lox_value_t * value = child->stack; value < child->esp; value++) {
            if(!traverse_value_and_move(*value)) {
                *data->moved_all_successfuly = false;
                return false;
            }
        }
    }

    return true;
}

static bool traverse_package_globals_to_move(void * trie_node_ptr, void * extra_ignored) {
    struct package * package = (struct package *) ((struct trie_node *) trie_node_ptr)->data;
    bool * moved_successfuly = (bool *) extra_ignored;

    for (int i = 0; i < package->global_variables.capacity && package->state != PENDING_COMPILATION; i++) {
        struct hash_table_entry * entry = &package->global_variables.entries[i];
        if (entry != NULL && entry->key != NULL) {
            if (!traverse_value_and_move(entry->value)) {
                *moved_successfuly = false;
                return false; //Stop iterating
            }
            if (!traverse_value_and_move(TO_LOX_VALUE_OBJECT(&entry->key->object))) {
                *moved_successfuly = false;
                return false; //Stop iterating
            }
        }
    }

    //Continue iteraring
    return true;
}

static bool traverse_value_and_move(lox_value_t root_value) {
    if (!IS_OBJECT(root_value)) {
        return true;
    }
    if (!belongs_to_young_generational_gc(current_vm.gc, (uintptr_t) AS_OBJECT(root_value))) {
        return true;
    }

    return traverse_object_and_move(AS_OBJECT(root_value));
}

static bool traverse_object_and_move(struct object * root_object) {
    struct generational_gc * generational_gc = current_vm.gc;
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, alloc_object_to_traverse(root_object, NULL));
    bool moved_all_successfuly = true;

    while (!is_empty_stack_list(&pending)) {
        struct object_to_traverse * to_traverse = pop_stack_list(&pending);
        struct object * current = to_traverse->object;
        free(to_traverse);

        if (!belongs_to_heap_generational_gc(generational_gc, (uintptr_t) current)) {
            continue;
        }
        if (can_be_moved(current)) {
            mark_object(current);
            //Failed move alloccation, start major gc
            if (!move_object(current)) {
                moved_all_successfuly = false;
                goto end;
            }

            switch (current->type) {
                case OBJ_STRUCT_INSTANCE:
                    traverse_lox_hashtable(((struct struct_instance_object *) current)->fields, &pending);
                    break;
                case OBJ_ARRAY:
                    traverse_lox_arraylist(((struct array_object *) current)->values, &pending);
                    break;
            }
        }
    }

    end:
    free_stack_list(&pending);

    return moved_all_successfuly;
}

static void traverse_lox_arraylist(struct lox_arraylist array_list, struct stack_list * pending) {
    for (int i = 0; i < array_list.in_use; i++) {
        if (IS_OBJECT(array_list.values[i])) {
            struct object * object = AS_OBJECT(array_list.values[i]);
            lox_value_t * reference_holder = array_list.values + i;
            push_stack_list(pending, alloc_object_to_traverse(object, reference_holder));
        }
    }
}

static void traverse_lox_hashtable_entry(
        struct string_object * key,
        struct string_object ** key_reference_holder,
        lox_value_t value,
        lox_value_t * reference_holder,
        void * extra
) {
    struct stack_list * pending = extra;
    if (IS_OBJECT(value)) {
        struct object * object = AS_OBJECT(value);
        push_stack_list(pending, alloc_object_to_traverse(object, reference_holder));
    }
}

static void traverse_lox_hashtable(struct lox_hash_table hash_table, struct stack_list * pending) {
    for_each_value_hash_table(&hash_table, pending, traverse_lox_hashtable_entry);
}

static bool can_be_moved(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;

    return belongs_to_young_generational_gc(generational_gc, object_ptr) &&
           !is_marked_generational_gc(generational_gc, object_ptr); //Not already moved
}

static void mark_object(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;
    struct mark_bitmap * mark_bitmap = get_mark_bitmap_generational_gc(generational_gc, object_ptr);
    set_marked_bitmap(mark_bitmap, (uint64_t *) object);
}

static bool move_object(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;

    uint8_t * new_ptr = move_to_new_space(object);
    bool moved_successfully = new_ptr != NULL;

    if (moved_successfully) {
        SET_FORWARDING_PTR(object, TO_LOX_VALUE_OBJECT(new_ptr));
        CLEAR_FORWARDING_PTR((struct object *) new_ptr);
        fix_object_inner_pointers_when_moved((struct object *) new_ptr);
    }

    return moved_successfully;
}

static uint8_t * move_to_new_space(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;

    INCREMENT_GENERATION(object);

    if (GET_GENERATION(object) >= config.generational_gc_config.n_generations_to_old) {
        return move_to_old(generational_gc->old, object);
    } else {
        return move_to_survivor_space(generational_gc->survivor, object);
    }
}

static struct object_to_traverse * alloc_object_to_traverse(struct object * object, lox_value_t * reference_holder) {
    struct object_to_traverse * to_traverse = malloc(sizeof(struct object_to_traverse));
    to_traverse->reference_holder = reference_holder;
    to_traverse->object = object;
    return to_traverse;
}

static void signal_threads_start_gc_alg_and_await() {
    struct generational_gc * gc = current_vm.gc;

    //Threads who are waiting, are included value_as already ack the start gc signal
    // + 1 to count self thread44
    atomic_fetch_add(&gc->number_threads_ack_start_gc_signal, current_vm.number_waiting_threads + 1);

    await_all_threads_signal_start_gc();

    gc->state = GC_IN_PROGRESS;
}

static void await_all_threads_signal_start_gc() {
    struct generational_gc * gc = current_vm.gc;

    lock_mutex(&gc->await_ack_start_gc_signal_mutex);

    while(current_vm.number_current_threads > gc->number_threads_ack_start_gc_signal){
        pthread_cond_wait(&gc->await_ack_start_gc_signal_cond,
                          &gc->await_ack_start_gc_signal_mutex.native_mutex);
    }

    unlock_mutex(&gc->await_ack_start_gc_signal_mutex);
}

static void signal_threads_gc_finished_alg() {
    struct generational_gc * gc = current_vm.gc;

    pthread_cond_broadcast(&gc->await_gc_cond);
}