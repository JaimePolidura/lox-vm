#include "runtime/memory/generational/generational_gc.h"
#include "runtime/memory/generational/utils.h"

#include "runtime/threads/vm_thread.h"
#include "runtime/vm.h"

#include "shared/config/config.h"

extern struct vm current_vm;
extern struct trie_list * compiled_packages;
extern struct config config;

struct object_to_traverse {
    lox_value_t * reference_holder;
    struct object * object;
};

static struct object_to_traverse * alloc_object_to_traverse(struct object * object, lox_value_t * reference_holder);
static void traverse_thread_stack_to_move(struct vm_thread *, struct vm_thread *, int index, void *);
static void traverse_heap_and_move(struct stack_list * terminated_threads);
static void traverse_thread_stack_to_update_references(struct vm_thread *, struct vm_thread *, int index, void *);
static void traverse_package_globals_to_move(void *, void *);
static void traverse_value_and_update_references(lox_value_t root_value, lox_value_t * root_reference_holder);
static void traverse_value_and_move(lox_value_t root_value);
static bool can_be_moved(struct object * object);
static void mark_object(struct object * object);
static void move_object(struct object * object);
static void traverse_lox_arraylist(struct lox_arraylist array_list, struct stack_list * pending);
static void traverse_lox_hashtable(struct lox_hash_table hash_table, struct stack_list * pending);
static uint8_t * move_to_new_space(struct object *);
static void update_references();
static void reset_bitmaps();
static void remove_terminated_threads(struct stack_list * terminated_threads);
static void update_object_references(struct object * object);
static void update_lox_value_reference(lox_value_t * value);
static bool has_been_updated(struct object *);
static void mark_as_updated(struct object *);
static void traverse_package_globals_to_update_references(void * trie_node_ptr, void * extra_ignored);

void start_minor_generational_gc() {
    struct generational_gc * generational_gc = current_vm.gc;
    struct stack_list terminated_threads;
    init_stack_list(&terminated_threads);

    traverse_heap_and_move(&terminated_threads);
    update_references();

    swap_from_to_survivor_space(generational_gc->survivor);
    reset_bitmaps();
    remove_terminated_threads(&terminated_threads);
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
}

static void traverse_package_globals_to_update_references(void * trie_node_ptr, void * extra_ignored) {
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
}

static void traverse_thread_stack_to_update_references(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    for(lox_value_t * value = child->stack; value < child->esp; value++) {
        if (IS_OBJECT(*value)) {
            traverse_value_and_update_references(*value, value);
        }
    }
}

static void traverse_value_and_update_references(lox_value_t root_value, lox_value_t * root_reference_holder) {
    struct generational_gc * generational_gc = current_vm.gc;
    struct object * root_object = AS_OBJECT(root_value);
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, alloc_object_to_traverse(root_object, root_reference_holder));

    while (!is_empty_stack_list(&pending)) {
        struct object_to_traverse * to_traverse = pop_stack_list(&pending);
        lox_value_t * current_reference_holder = to_traverse->reference_holder;
        struct object * current = to_traverse->object;
        free(to_traverse);

        if(!has_been_updated(current)){
            lox_value_t * current_forwading_ptr = GET_FORWARDING_PTR(current);

            mark_as_updated(current);

            if(current_forwading_ptr != NULL){
                *current_reference_holder = *current_forwading_ptr;
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

static bool has_been_updated(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;

    if (belongs_to_eden(generational_gc->eden, object_ptr)) {
        return is_marked_bitmap(generational_gc->eden->updated_references_mark_bitmap, object_ptr);
    } else if (belongs_to_survivor(generational_gc->survivor, (uintptr_t) object)) {
        return is_marked_bitmap(generational_gc->survivor->fromspace_updated_references_mark_bitmap, object_ptr);
    } else { //Old
        return is_marked_bitmap(generational_gc->old->updated_references_mark_bitmap, object_ptr);
    }
}

static void mark_as_updated(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;

    if (belongs_to_eden(generational_gc->eden, object_ptr)) {
        set_marked_bitmap(generational_gc->eden->updated_references_mark_bitmap, object_ptr);
    } else if (belongs_to_survivor(generational_gc->survivor, (uintptr_t) object)) {
        set_marked_bitmap(generational_gc->survivor->fromspace_updated_references_mark_bitmap, object_ptr);
    } else { //Old
        set_marked_bitmap(generational_gc->old->updated_references_mark_bitmap, object_ptr);
    }
}

static void reset_bitmaps() {
    struct generational_gc * generational_gc = current_vm.gc;

    reset_mark_bitmap(generational_gc->eden->updated_references_mark_bitmap);
    reset_mark_bitmap(generational_gc->eden->moved_mark_bitmap);
    reset_mark_bitmap(generational_gc->survivor->fromspace_updated_references_mark_bitmap);
    reset_mark_bitmap(generational_gc->survivor->fromspace_moved_mark_bitmap);
    reset_mark_bitmap(generational_gc->old->updated_references_mark_bitmap);
}

static void traverse_heap_and_move(struct stack_list * terminated_threads) {
    //Thread stacks
    for_each_thread(current_vm.root, traverse_thread_stack_to_move, terminated_threads,
                    THREADS_OPT_INCLUDE_TERMINATED |
                    THREADS_OPT_RECURSIVE |
                    THREADS_OPT_INCLUSIVE);
    //Globals
    for_each_node(compiled_packages, NULL, traverse_package_globals_to_move);
}

static void traverse_thread_stack_to_move(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    if(child->state == THREAD_TERMINATED && child->terminated_state == THREAD_TERMINATED_PENDING_GC) {
        struct stack_list * terminated_threads = terminated_threads_ptr;
        push_stack_list(terminated_threads, child);
    } else {
        for(lox_value_t * value = child->stack; value < child->esp; value++) {
            traverse_value_and_move(*value);
        }
    }
}

static void traverse_package_globals_to_move(void * trie_node_ptr, void * extra_ignored) {
    struct package * package = (struct package *) ((struct trie_node *) trie_node_ptr)->data;

    for(int i = 0; i < package->global_variables.capacity && package->state != PENDING_COMPILATION; i++) {
        struct hash_table_entry * entry = &package->global_variables.entries[i];
        if(entry != NULL && entry->key != NULL){
            traverse_value_and_move(entry->value);
            traverse_value_and_move(TO_LOX_VALUE_OBJECT(&entry->key->object));
        }
    }
}

static void traverse_value_and_move(lox_value_t root_value) {
    if (!IS_OBJECT(root_value)) {
        return;
    }

    struct generational_gc * generational_gc = current_vm.gc;
    struct object * root_object = AS_OBJECT(root_value);
    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, alloc_object_to_traverse(root_object, NULL));

    while (!is_empty_stack_list(&pending)) {
        struct object_to_traverse * to_traverse = pop_stack_list(&pending);
        struct object * current = to_traverse->object;
        free(to_traverse);

        if (can_be_moved(current)) {
            mark_object(current);
            move_object(current);

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

static void traverse_lox_arraylist(struct lox_arraylist array_list, struct stack_list * pending) {
    for (int i = 0; i < array_list.in_use; i++) {
        if (IS_OBJECT(array_list.values[i])) {
            struct object * object = AS_OBJECT(array_list.values[i]);
            lox_value_t * reference_holder = array_list.values + i;
            push_stack_list(pending, alloc_object_to_traverse(object, reference_holder));
        }
    }
}

static void traverse_lox_hashtable_entry(lox_value_t value, lox_value_t * reference_holder, void * extra) {
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

    bool belongs_to_eden_or_survivor = belongs_to_survivor(generational_gc->survivor, object_ptr) ||
                                       belongs_to_eden(generational_gc->eden, object_ptr);
    bool it_hasnt_already_been_moved = !is_marked_bitmap(generational_gc->eden->moved_mark_bitmap, object_ptr) &&
                                       !is_marked_bitmap(generational_gc->survivor->fromspace_moved_mark_bitmap, object_ptr);

    return belongs_to_eden_or_survivor && it_hasnt_already_been_moved;
}

static void mark_object(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;

    if (belongs_to_eden(generational_gc->eden, object_ptr)) {
        set_marked_bitmap(generational_gc->eden->moved_mark_bitmap, object_ptr);
    } else {
        set_marked_bitmap(generational_gc->survivor->fromspace_moved_mark_bitmap, object_ptr);
    }
}

static void move_object(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    struct memory_space * to_space = generational_gc->survivor->to;

    uint8_t * new_ptr = move_to_new_space(object);
    SET_FORWARDING_PTR(object, new_ptr);
    SET_FORWARDING_PTR((struct object *) new_ptr, object);
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