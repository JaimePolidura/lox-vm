#include "old.h"

struct old * alloc_old(struct config config) {
    struct old * old = malloc(sizeof(struct old));
    size_t size_old_bytes = config.generational_gc_config.old_size_mb * 1024 * 1024;
    init_memory_space(&old->memory_space, size_old_bytes);
    return old;
}

uint8_t * move_to_old(struct old * old, struct object * object) {
    int n_bytes_allocated = get_n_bytes_allocated_object(object);
    return copy_data_memory_space(&old->memory_space, (uint8_t *) object, n_bytes_allocated);
}
