#include "shared/types/array_object.h"

#include "runtime/memory/generational/generational_gc.h"
#include "runtime/vm.h"

#include "shared.h"

extern struct vm current_vm;

static void mark_dirty_card_table_generational_gc(uintptr_t young);

void write_barrier_generational_gc(struct object * object_dst, struct object * src) {
    struct generational_gc * gc = current_vm.gc;
    if (belongs_to_old(gc->old, (uintptr_t) object_dst) && belongs_to_young_generational_gc(gc, (uintptr_t) src)) {
        mark_dirty_card_table_generational_gc((uintptr_t) src);
    }
}


static void mark_dirty_card_table_generational_gc(uintptr_t young) {
    struct generational_gc * gc = current_vm.gc;
    struct card_table * card_table = get_card_table_generational_gc(gc, (uintptr_t) young);
    mark_dirty_card_table(card_table, (uint64_t *) young);
}