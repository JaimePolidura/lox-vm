#include "survivor.h"

struct survivor * alloc_survivor(struct config config) {
    size_t size_in_bytes = config.generational_gc_config.survivor_size_mb * 1024 * 1024;

    struct survivor * survivor = NATIVE_LOX_MALLOC(sizeof(struct survivor));
    survivor->from = alloc_memory_space(size_in_bytes);
    survivor->to = alloc_memory_space(size_in_bytes);

    int n_addresses = (int) round_up_8(size_in_bytes / 8);
    init_card_table(&survivor->fromspace_card_table, config, (uint64_t *) survivor->from->start, (uint64_t *) survivor->from->end);
    init_mark_bitmap(&survivor->fromspace_mark_bitmap, n_addresses, survivor->from->start);

    return survivor;
}

void swap_from_to_survivor_space(struct survivor * survivor, struct config config) {
    struct memory_space * from_memory_space = survivor->from;
    size_t size_in_bytes = config.generational_gc_config.survivor_size_mb * 1024 * 1024;
    int n_addresses = (int) round_up_8(size_in_bytes / 8);

    survivor->from = survivor->to;
    survivor->to = from_memory_space;

    reset_memory_space(survivor->to);
    //Card tables already cleaned in minor_gc
    init_card_table(&survivor->fromspace_card_table, config, (uint64_t *) survivor->from->start, (uint64_t *) survivor->from->end);

    survivor->fromspace_mark_bitmap.start_address = survivor->from->start;
}

bool belongs_to_survivor(struct survivor * survivor, uintptr_t ptr) {
    bool belongs_to_from_space = belongs_to_memory_space(survivor->from, ptr);
    bool belongs_to_to_space = belongs_to_memory_space(survivor->to, ptr);

    return belongs_to_from_space || belongs_to_to_space;
}

uint8_t * move_to_survivor_space(struct survivor * survivor, struct object * object) {
    int n_bytes_allocated = get_n_bytes_allocated_object(object);
    return copy_data_memory_space(survivor->to, (uint8_t *) object, n_bytes_allocated);
}