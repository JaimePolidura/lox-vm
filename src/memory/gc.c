#include "gc.h"

extern struct string_pool global_string_pool;
extern struct vm current_vm;

static void mark_stack();
static void mark_globals();
static void traverse_root_dependences();

static void mark_value(lox_value_t * value);
static void mark_object(struct object * object);
static void mark_array(struct lox_array * array);
static void sweep();
static void sweep_heap();
static void sweep_string_pool();

void start_gc() {
    mark_stack();
    mark_globals();
    traverse_root_dependences();

    sweep();
}

static void mark_stack() {
    for(lox_value_t * value = current_vm.stack; value < current_vm.esp; value++){
        mark_value(value);
    }
}

static void mark_globals() {
    for(int i = 0; i < current_vm.global_variables.capacity; i++) {
        struct hash_table_entry * entry = &current_vm.global_variables.entries[i];
        mark_value(&entry->value);
        mark_object(&entry->key->object);
    }
}

static void traverse_root_dependences() {
    while(current_vm.gc_info.gray_count > 0){
        struct object * object = current_vm.gc_info.gray_stack[--current_vm.gc_info.gray_count];

        switch (object->type) {
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
    struct object * previous = NULL;
    struct object * object = current_vm.gc_info.heap;

    while (object != NULL) {
        if (object->gc_marked) {
            object->gc_marked = false;
            previous = object;
            object = object->next;
        } else {
            struct object *unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                current_vm.gc_info.heap = object;
            }

            current_vm.gc_info.bytes_allocated -= unreached;
            free(unreached);
        }
    }
}

static void sweep_string_pool() {
    for(int i = 0; i < global_string_pool.strings.size; i++) {
        struct hash_table_entry * entry = &global_string_pool.strings.entries[i];
        if(entry != NULL && !entry->value.as.object->gc_marked){
            remove_entry_hash_table(entry);
        }
    }
}

static void mark_array(struct lox_array * array) {
    for(int i = 0; i < array->in_use; i++){
        mark_value(&array->values[i]);
    }
}

static void mark_value(lox_value_t * value) {
    if(IS_OBJECT(*value)){
        mark_object(value->as.object);
    }
}

static void mark_object(struct object * object) {
    object->gc_marked = true;
}