#include "runtime/memory/generational/generational_gc.h"
#include "runtime/memory/generational/utils.h"
#include "runtime/vm.h"

extern struct trie_list * compiled_packages;
extern struct vm current_vm;

static void mark(struct stack_list * terminated_threads);
static bool traverse_thread_stack_to_mark(struct vm_thread *, struct vm_thread *, int, void *);
static bool traverse_globals_to_mark(void *, void *);
static void traverse_value_to_mark(lox_value_t value);
static void traverse_object_to_mark(struct object *);
static void traverse_lox_hashtable(struct stack_list *, struct lox_hash_table);
static void traverse_lox_arraylist(struct stack_list *, struct lox_arraylist);
static bool can_be_marked(uintptr_t ptr);

static void compact_old();
static void update_references();
static struct object * next_scan(struct old *, struct object * prev);
static struct object * next_free(struct old *, struct object * prev);
static size_t free_size(struct old *, struct object * ptr);
static void move_compacted_objects(struct old *, struct object * free, struct object * scan);

static bool traverse_thread_stack_to_update_references(struct vm_thread *, struct vm_thread *, int, void *);
static bool traverse_globals_to_update_references(void * trie_node_ptr, void * extra_ignored);
static void traverse_value_and_update_references(lox_value_t * root_value);
static bool can_reference_be_updated(lox_value_t * value);

void start_major_generational_gc() {
    struct generational_gc * gc = current_vm.gc;
    struct stack_list terminated_threads;
    init_stack_list(&terminated_threads);

    clear_mark_bitmaps_generational_gc(gc);
    mark(&terminated_threads);
    compact_old();
    clear_mark_bitmaps_generational_gc(gc);
    update_references();
}

static void update_references() {
    //Thread stacks
    for_each_thread(current_vm.root, traverse_thread_stack_to_update_references, NULL,
                    THREADS_OPT_INCLUDE_TERMINATED |
                    THREADS_OPT_RECURSIVE |
                    THREADS_OPT_INCLUSIVE);

    //Globals
    for_each_node(compiled_packages, NULL, traverse_globals_to_update_references);
}

static bool traverse_thread_stack_to_update_references(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    for(lox_value_t * value = child->stack; value < child->esp; value++) {
        if (IS_OBJECT(*value)) {
            traverse_value_and_update_references(value);
        }
    }

    return true;
}

static bool traverse_globals_to_update_references(void * trie_node_ptr, void * extra_ignored) {
    struct package * package = (struct package *) ((struct trie_node *) trie_node_ptr)->data;

    for (int i = 0; i < package->global_variables.capacity && package->state != PENDING_COMPILATION; i++) {
        struct hash_table_entry * entry = &package->global_variables.entries[i];
        if (entry != NULL && entry->key != NULL) {
            traverse_value_and_update_references(&entry->value);
        }
    }

    //Continue iteraring
    return true;
}

static void traverse_value_and_update_references(lox_value_t * root_value) {
    struct generational_gc * gc = current_vm.gc;
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, root_value);

    while (!is_empty_stack_list(&pending)) {
        lox_value_t * current_value = pop_stack_list(&pending);

        if (can_reference_be_updated(current_value)) {
            uintptr_t current_value_ptr = (uintptr_t) AS_OBJECT(* current_value);
            struct mark_bitmap * mark_bit_map = get_mark_bitmap_generational_gc(gc, current_value_ptr);
            set_marked_bitmap(mark_bit_map, current_value_ptr);
            struct object * current_object = AS_OBJECT(* current_value);
            void * forwading_ptr = GET_FORWARDING_PTR(current_object);

            if (forwading_ptr != NULL) {
                SET_FORWARDING_PTR(current_object, NULL);
                *current_value = TO_LOX_VALUE_OBJECT((struct object *) forwading_ptr);
            }

            switch (current_object->type) {
                case OBJ_STRUCT_INSTANCE:
                    traverse_lox_hashtable(&pending, ((struct struct_instance_object *) current_object)->fields);
                    break;
                case OBJ_ARRAY:
                    traverse_lox_arraylist(&pending, ((struct array_object *) current_object)->values);
                    break;
            }
        }
    }

    free_stack_list(&pending);
}

static bool can_reference_be_updated(lox_value_t * value) {
    struct generational_gc * gc = current_vm.gc;
    uintptr_t current_value_ptr = (uintptr_t) AS_OBJECT(* value);
    struct mark_bitmap * mark_bit_map = get_mark_bitmap_generational_gc(gc, current_value_ptr);

    return belongs_to_heap_generational_gc(gc, current_value_ptr) &&
        is_marked_bitmap(mark_bit_map, current_value_ptr);
}

static void compact_old() {
    struct generational_gc * gc = current_vm.gc;
    struct old * old = gc->old;

    struct object * free = ((struct object *) old->memory_space.end) - 1;
    struct object * scan = (struct object *) old->memory_space.start;

    while (free < scan) {
        scan = next_scan(old, scan);
        free = next_free(old, free);

        if (free > scan) {
            size_t available_free_size = free_size(old, free);
            size_t to_move_req_size = get_n_bytes_allocated_object((struct object *) scan);

            if (available_free_size >= to_move_req_size) {
                move_compacted_objects(old, free, scan);
            } else {
                scan -= 1;
            }
        }
    }
}

static void move_compacted_objects(struct old * old, struct object * free, struct object * scan) {
    size_t size = get_n_bytes_allocated_object(scan);

    copy_data_at_memory_space(&old->memory_space, (uint8_t *) free, (uint8_t *) scan, size);

    //Store forwading pointer
    scan->gc_info = (void * ) free;

    set_marked_bitmap(old->mark_bitmap, (uint64_t) free);
    set_unmarked_bitmap(old->mark_bitmap, (uint64_t) scan);
}

static size_t free_size(struct old * old, struct object * ptr) {
    struct object * start = ptr;
    struct object * end = ptr;

    while (!is_marked_bitmap(old->mark_bitmap, (uint64_t) end)) {
        end++;
    }

    return end - start;
}

static struct object * next_scan(struct old * old, struct object * prev) {
    struct object * current_scan = prev;

    while (!is_marked_bitmap(old->mark_bitmap, (uintptr_t) current_scan) &&
        ((uint8_t *) current_scan) >= old->memory_space.start) {

        current_scan -= 1;
    }

    return current_scan;
}

static struct object * next_free(struct old * old, struct object * prev) {
    struct object * current_free = prev;

    while (is_marked_bitmap(old->mark_bitmap, (uintptr_t) current_free) &&
           ((uint8_t *) current_free) < old->memory_space.end) {
        current_free += 1;
    }

    return current_free;
}

static void mark(struct stack_list * terminated_threads) {
    struct generational_gc * gc = current_vm.gc;

    //Thread stacks
    for_each_thread(current_vm.root, traverse_thread_stack_to_mark, terminated_threads,
                    THREADS_OPT_INCLUDE_TERMINATED |
                    THREADS_OPT_RECURSIVE |
                    THREADS_OPT_INCLUSIVE);

    //Globals
    for_each_node(compiled_packages, NULL, traverse_globals_to_mark);
}

static bool traverse_globals_to_mark(void * trie_node_ptr, void * extra_ignored) {
    struct package * package = (struct package *) ((struct trie_node *) trie_node_ptr)->data;

    for (int i = 0; i < package->global_variables.capacity && package->state != PENDING_COMPILATION; i++) {
        struct hash_table_entry * entry = &package->global_variables.entries[i];
        if (entry != NULL && entry->key != NULL) {
            traverse_value_to_mark(entry->value);
            traverse_object_to_mark((struct object *) entry->key);
        }
    }

    //Continue iteraring
    return true;
}

static bool traverse_thread_stack_to_mark(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    struct stack_list * terminated_threads = terminated_threads_ptr;

    if (child->state == THREAD_TERMINATED && child->terminated_state == THREAD_TERMINATED_PENDING_GC) {
        push_stack_list(terminated_threads, child);
    } else {
        for(lox_value_t * value = child->stack; value < child->esp; value++) {
            traverse_value_to_mark(*value);
        }
    }

    return true;
}

static void traverse_value_to_mark(lox_value_t root_value) {
    if (IS_OBJECT(root_value)) {
        traverse_object_to_mark(AS_OBJECT(root_value));
    }
}

static void traverse_object_to_mark(struct object * object) {
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, object);
    struct generational_gc * gc = current_vm.gc;

    while (!is_empty_stack_list(&pending)) {
        struct object * current = pop_stack_list(&pending);
        uintptr_t current_ptr = (uintptr_t) current;
        struct mark_bitmap * mark_bitmap = get_mark_bitmap_generational_gc(gc, current_ptr);

        if (can_be_marked(current_ptr)) {
            set_marked_bitmap(mark_bitmap, current_ptr);

            switch (current->type) {
                case OBJ_STRUCT_INSTANCE:
                    traverse_lox_hashtable(&pending, ((struct struct_instance_object *) current)->fields);
                    break;
                case OBJ_ARRAY:
                    traverse_lox_arraylist(&pending, ((struct array_object *) current)->values);
                    break;
            }
        }
    }

    free_stack_list(&pending);
}

static bool can_be_marked(uintptr_t ptr) {
    struct generational_gc * gc = current_vm.gc;
    return belongs_to_heap_generational_gc(gc, ptr) &&
        !is_marked_bitmap(get_mark_bitmap_generational_gc(gc, ptr), ptr);
}

static void traverse_lox_hashtable_entry(lox_value_t value, lox_value_t * reference_holder, void * extra) {
    struct stack_list * pending = extra;
    if (IS_OBJECT(value)) {
        struct object * object = AS_OBJECT(value);
        push_stack_list(pending, object);
    }
}

static void traverse_lox_hashtable(struct stack_list * pending, struct lox_hash_table hash_table) {
    for_each_value_hash_table(&hash_table, pending, traverse_lox_hashtable_entry);
}

static void traverse_lox_arraylist(struct stack_list * pending, struct lox_arraylist array_list) {
    for (int i = 0; i < array_list.in_use; i++) {
        if (IS_OBJECT(array_list.values[i])) {
            struct object * object = AS_OBJECT(array_list.values[i]);
            lox_value_t * reference_holder = array_list.values + i;
            push_stack_list(pending, object);
        }
    }
}