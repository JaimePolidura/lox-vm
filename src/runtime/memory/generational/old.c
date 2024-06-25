#include "old.h"

struct old * alloc_old(struct config config) {
    struct old * old = malloc(sizeof(struct old));
    size_t size_old_in_bytes = config.generational_gc_config.old_size_mb * 1024 * 1024;
    init_memory_space(&old->memory_space, size_old_in_bytes);

    int n_addresses = (int) round_up_8(size_old_in_bytes / 8);
    old->mark_bitmap = alloc_mark_bitmap(n_addresses, (uint64_t) old->memory_space.start);

    return old;
}

uint8_t * move_to_old(struct old * old, struct object * object) {
    int n_bytes_allocated = get_n_bytes_allocated_object(object);
    return copy_data_memory_space(&old->memory_space, (uint8_t *) object, n_bytes_allocated);
}

bool belongs_to_old(struct old * old, uintptr_t ptr) {
    return belongs_to_memory_space(&old->memory_space, ptr);
}