#include "runtime/memory/generational/generational_gc.h"
#include "runtime/memory/generational/utils.h"

#include "runtime/threads/vm_thread.h"
#include "runtime/vm.h"

#include "shared/config/config.h"

extern struct vm current_vm;
extern struct trie_list * compiled_packages;
extern struct config config;

static void traverse_heap_and_move(struct stack_list * terminated_threads);
static void traverse_thread_stack(struct vm_thread *parent, struct vm_thread *child, int index, void *terminated_threads_ptr);
static void traverse_package_globals(void *, void *);
static void traverse_value_and_move(lox_value_t root_value);
static bool can_be_moved(struct object * object);
static void mark_object(struct object * object);
static void move_object(struct object * object);
static void traverse_lox_arraylist(struct lox_arraylist array_list, struct stack_list * pending);
static void traverse_lox_hashtable(struct lox_hash_table hash_table, struct stack_list * pending);
static uint8_t * move_to_new_space(struct object *);
static void update_references();
static void reset_mark_bitmaps();

void start_minor_generational_gc() {
    struct generational_gc * generational_gc = current_vm.gc;
    struct stack_list terminated_threads;
    init_stack_list(&terminated_threads);

    traverse_heap_and_move(&terminated_threads);
    update_references();

    swap_from_to_survivor_space(generational_gc->survivor);
    reset_mark_bitmaps();
}

static void update_references() {

}

static void reset_mark_bitmaps() {

}

static void traverse_heap_and_move(struct stack_list * terminated_threads) {
    struct stack_list roots;
    init_stack_list(&roots);

    //Move thread stacks
    for_each_thread(current_vm.root, traverse_thread_stack, terminated_threads,
                    THREADS_OPT_INCLUDE_TERMINATED |
                    THREADS_OPT_RECURSIVE |
                    THREADS_OPT_INCLUSIVE);
    //Move globals
    for_each_node(compiled_packages, NULL, traverse_package_globals);
}

static void traverse_thread_stack(struct vm_thread * parent, struct vm_thread * child, int index, void * terminated_threads_ptr) {
    if(child->state == THREAD_TERMINATED && child->terminated_state == THREAD_TERMINATED_PENDING_GC) {
        struct stack_list * terminated_threads = terminated_threads_ptr;
        push_stack_list(terminated_threads, child);
    } else {
        for(lox_value_t * value = child->stack; value < child->esp; value++) {
            traverse_value_and_move(*value);
        }
    }
}

static void traverse_package_globals(void * trie_node_ptr, void * extra_ignored) {
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
    push_stack_list(&pending, root_object);

    while (!is_empty_stack_list(&pending)) {
        struct object * current = pop_stack_list(&pending);

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

static bool can_be_moved(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;

    bool belongs_to_eden_or_survivor = belongs_to_survivor(generational_gc->survivor, object_ptr) ||
                                       belongs_to_eden(generational_gc->eden, object_ptr);
    bool it_hasnt_already_been_moved = !is_marked_bitmap(generational_gc->eden->mark_bitmap, object_ptr) &&
            !is_marked_bitmap(generational_gc->survivor->from->mark_bitmap, object_ptr);

    return belongs_to_eden_or_survivor && it_hasnt_already_been_moved;
}

static void mark_object(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    uintptr_t object_ptr = (uintptr_t) object;

    if (belongs_to_eden(generational_gc->eden, object_ptr)) {
        set_marked_bitmap(generational_gc->eden->mark_bitmap, object_ptr);
    } else {
        set_marked_bitmap(generational_gc->survivor->from->mark_bitmap, object_ptr);
    }
}

static void traverse_lox_arraylist(struct lox_arraylist array_list, struct stack_list * pending) {
    for(int i = 0; i < array_list.in_use; i++){
        if(IS_OBJECT(array_list.values[i])){
            push_stack_list(pending, AS_OBJECT(array_list.values[i]));
        }
    }
}

static void traverse_lox_hashtable_entry(lox_value_t value, void * extra) {
    struct stack_list * pending = extra;
    if(IS_OBJECT(value)) {
        push_stack_list(pending, AS_OBJECT(value));
    }
}

static void traverse_lox_hashtable(struct lox_hash_table hash_table, struct stack_list * pending) {
    for_each_value_hash_table(&hash_table, pending, traverse_lox_hashtable_entry);
}

static void move_object(struct object * object) {
    struct generational_gc * generational_gc = current_vm.gc;
    struct survivor_space * to_space = generational_gc->survivor->to;

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
        return move_to_survivor_space(generational_gc->survivor->to, object);
    }
}