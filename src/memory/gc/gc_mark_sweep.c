#include "gc_mark_sweep.h"

extern __thread struct vm_thread * self_thread;
extern struct string_pool global_string_pool;
extern struct trie_list * compiled_packages;
extern struct vm current_vm;

static void for_each_package_callback(void * package_ptr);
static void mark_thread_stack(struct vm_thread * parent, struct vm_thread * child, int index);
static void await_all_threads_signal_start_gc();
static void notify_start_gc_signal_acked();
static void await_until_gc_finished();

static void traverse_root_dependences(struct gc_mark_sweep * gc_mark_sweep);
static void add_value_gc_info(struct object * value);
static void mark_globals();
static void mark_stack();
static void mark_value(lox_value_t * value);
static void mark_object(struct object * object);
static void mark_array(struct lox_array * array);
static void mark_hash_table(struct hash_table * table);

static void sweep();
static void sweep_heap();
static void sweep_string_pool();
static void sweep_heap_thread(struct vm_thread * parent_ignore, struct vm_thread * vm_thread, int index);

void signal_threads_start_gc_alg() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    self_thread->state = THREAD_WAITING;

    lock_writer_rw_mutex(&current_vm.blocking_call_mutex);
    //Threads who are waiting, are included as already ack the start gc signal
    gc_mark_sweep->number_threads_ack_start_gc_signal = current_vm.number_waiting_threads;
    unlock_writer_rw_mutex(&current_vm.blocking_call_mutex);

    await_all_threads_signal_start_gc();

    self_thread->state = THREAD_RUNNABLE;
}

void signal_threads_gc_finished_alg() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    pthread_cond_signal(&gc_mark_sweep->await_gc_cond);
}

void check_gc_on_safe_point_alg() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;
    gc_state_t current_gc_state = current_vm.gc.state;

    switch (current_gc_state) {
        case GC_NONE: return;
        case GC_WAITING: atomic_fetch_add(&gc_mark_sweep->number_threads_ack_start_gc_signal, 1);
        case GC_IN_PROGRESS:
            set_self_thread_waiting();

            notify_start_gc_signal_acked();
            await_until_gc_finished();

            set_self_thread_runnable();
    }
}

void init_gc_alg() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;
    gc_mark_sweep->gray_stack = NULL;
    gc_mark_sweep->gray_capacity = 0;
    gc_mark_sweep->gray_count = 0;

    gc_mark_sweep->number_threads_ack_start_gc_signal = 0;
    pthread_cond_init(&gc_mark_sweep->await_ack_start_gc_signal_cond, NULL);
    init_mutex(&gc_mark_sweep->await_ack_start_gc_signal_mutex);
    pthread_cond_init(&gc_mark_sweep->await_gc_cond, NULL);
    init_mutex(&gc_mark_sweep->await_gc_cond_mutex);
}

void start_gc_alg() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    mark_stack();
    mark_globals();
    traverse_root_dependences(gc_mark_sweep);

    sweep();
}

static void mark_stack() {
    for_each_thread(current_vm.root, mark_thread_stack, THREADS_OPT_INCLUDE_TERMINATED | THREADS_OPT_RECURSIVE | THREADS_OPT_INCLUSIVE);
}

static void mark_thread_stack(struct vm_thread * parent, struct vm_thread * child, int index) {
    if(child->state == THREAD_TERMINATED){
        free_vm_thread(child);
        free(child);
        parent->children[index] = NULL;
    } else {
        for(lox_value_t * value = child->stack; value < child->esp; value++) {
            mark_value(value);
        }
    }
}

static void mark_globals() {
    for_each_node(compiled_packages, for_each_package_callback);
}

static void for_each_package_callback(void * package_ptr) {
    struct package * package = (struct package *) package_ptr;

    for(int i = 0; i < package->global_variables.capacity && package->state != PENDING_COMPILATION; i++) {
        struct hash_table_entry * entry = &package->global_variables.entries[i];
        mark_value(&entry->value);
        mark_object(&entry->key->object);
    }
}

static void traverse_root_dependences(struct gc_mark_sweep * gc_mark_sweep) {
    while(gc_mark_sweep->gray_count > 0){
        struct object * object = gc_mark_sweep->gray_stack[--gc_mark_sweep->gray_count];

        switch (object->type) {
            case OBJ_STRUCT_INSTANCE: {
                struct struct_instance_object * struct_object = (struct struct_instance_object *) object;
                mark_hash_table(&struct_object->fields);
                mark_object(&struct_object->object);
                break;
            }
            case OBJ_FUNCTION: {
                struct function_object * function = (struct function_object *) object;
                mark_object((struct object *) function->name);
                mark_array(&function->chunk.constants);
                break;
            }
            default: //The rest of objects doesn't contain any dependencies
                break;
        }
    }
}

static void sweep() {
    sweep_string_pool();
    sweep_heap();
}

static void sweep_heap() {
    for_each_thread(current_vm.root, sweep_heap_thread, THREADS_OPT_RECURSIVE | THREADS_OPT_INCLUSIVE);
}

static void sweep_heap_thread(struct vm_thread * parent_ignore, struct vm_thread * vm_thread, int index) {
    struct gc_thread_info * gc_info = &vm_thread->gc_info;

    struct object * object = gc_info->heap;
    struct object * previous = NULL;

    while (object != NULL) {
        if (object->gc_marked) {
            object->gc_marked = false;
            previous = object;
            object = object->next;
        } else {
            struct object * unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                gc_info->heap = object;
            }

            gc_info->bytes_allocated -= sizeof_heap_allocated_lox(object);
            free(unreached);
        }
    }
}

static void sweep_string_pool() {
    for(int i = 0; i < global_string_pool.strings.size; i++) {
        struct hash_table_entry * entry = &global_string_pool.strings.entries[i];
        if(entry != NULL && AS_OBJECT(entry->value)->gc_marked){
            remove_entry_hash_table(entry);
        }
    }
}

static void mark_hash_table_entry(lox_value_t value) {
    mark_value(&value);
}

static void mark_hash_table(struct hash_table * table) {
    for_each_value_hash_table(table, mark_hash_table_entry);
}

static void mark_array(struct lox_array * array) {
    for(int i = 0; i < array->in_use; i++){
        mark_value(&array->values[i]);
    }
}

static void mark_value(lox_value_t * value) {
    if(IS_OBJECT((*value))){
        mark_object(AS_OBJECT(*value));
    }
}

static void mark_object(struct object * object) {
    object->gc_marked = true;
    add_value_gc_info(object);
}

static void add_value_gc_info(struct object * value) {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    if (gc_mark_sweep->gray_capacity < gc_mark_sweep->gray_count + 1) {
        gc_mark_sweep->gray_capacity = GROW_CAPACITY(gc_mark_sweep->gray_capacity);
        gc_mark_sweep->gray_stack = realloc(gc_mark_sweep->gray_stack, sizeof(struct object*) * gc_mark_sweep->gray_capacity);
    }

    gc_mark_sweep->gray_stack[gc_mark_sweep->gray_count++] = value;
}

static void await_all_threads_signal_start_gc() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    lock_mutex(&gc_mark_sweep->await_ack_start_gc_signal_mutex);

    while(current_vm.number_current_threads > gc_mark_sweep->number_threads_ack_start_gc_signal ){
        pthread_cond_wait(&gc_mark_sweep->await_ack_start_gc_signal_cond,
                          &gc_mark_sweep->await_ack_start_gc_signal_mutex.native_mutex);
    }

    unlock_mutex(&gc_mark_sweep->await_ack_start_gc_signal_mutex);
}

static void await_until_gc_finished() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    lock_mutex(&gc_mark_sweep->await_gc_cond_mutex);

    while(current_vm.gc.state != GC_NONE){
        pthread_cond_wait(&gc_mark_sweep->await_gc_cond, &gc_mark_sweep->await_gc_cond_mutex.native_mutex);
    }

    unlock_mutex(&gc_mark_sweep->await_gc_cond_mutex);
}

static void notify_start_gc_signal_acked() {
    struct gc_mark_sweep * gc_mark_sweep = (struct gc_mark_sweep *) &current_vm.gc;

    lock_mutex(&gc_mark_sweep->await_ack_start_gc_signal_mutex);

    pthread_cond_signal(&gc_mark_sweep->await_ack_start_gc_signal_cond);

    unlock_mutex(&gc_mark_sweep->await_ack_start_gc_signal_mutex);
}