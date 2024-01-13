#include "gc.h"

extern struct vm current_vm;

static void mark_stack();
static void mark_globals();

static void mark_value(lox_value_t * value);
static void mark_object(struct object * object);

void start_gc() {
    mark_stack();
    mark_globals();
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

static void mark_value(lox_value_t * value) {
    if(IS_OBJECT(*value)){
        mark_object(value->as.object);
    }
}

static void mark_object(struct object * object) {
    object->gc_marked = true;
}