#include "shared/types/struct_instance_object.h"
#include "shared/types/array_object.h"

#include "runtime/memory/generational/generational_gc.h"
#include "runtime/vm.h"

#include "shared.h"

extern struct vm current_vm;

static void mark_dirty_card_table_generational_gc(uintptr_t young);

void write_struct_field_barrier_generational_gc(struct struct_instance_object * instance, struct object * new_value) {
    struct generational_gc * gc = current_vm.gc;
    if (belongs_to_old(gc->old, (uintptr_t) instance) && belongs_to_young(gc, (uintptr_t) new_value)) {
        mark_dirty_card_table_generational_gc((uintptr_t) new_value);
    }
}

void write_array_element_barrier_generational_gc(struct array_object * instance, struct object * new_value) {
    struct generational_gc * gc = current_vm.gc;
    if (belongs_to_old(gc->old, (uintptr_t) instance) && belongs_to_young(gc, (uintptr_t) new_value)) {
        mark_dirty_card_table_generational_gc((uintptr_t) new_value);
    }
}

static void mark_dirty_card_table_generational_gc(uintptr_t young) {
    struct generational_gc * gc = current_vm.gc;
    struct card_table * card_table = get_card_table(gc, (uintptr_t) young);
    mark_dirty_card_table(card_table, (uint64_t *) young);
}