#include "eden.h"

struct eden * alloc_eden(struct config config) {
    size_t size_blocks_in_bytes = config.generational_gc_config.eden_block_size_kb * 1024;
    size_t size_eden_in_bytes = config.generational_gc_config.eden_size_mb * 1024 * 1024;

    struct eden * eden = malloc(sizeof(struct eden));
    int n_addresses = (int) round_up_8(size_eden_in_bytes / 8);
    init_memory_space(&eden->memory_space, size_eden_in_bytes);
    eden->mark_bitmap = alloc_mark_bitmap(n_addresses, (uint64_t *) eden->memory_space.start);
    eden->size_blocks_in_bytes = size_blocks_in_bytes;
    init_card_table(&eden->card_table, config, (uint64_t *) eden->memory_space.start, (uint64_t *) eden->memory_space.end);

    return eden;
}

struct eden_thread * alloc_eden_thread() {
    struct eden_thread * eden_thread = malloc(sizeof(struct eden_thread));
    eden_thread->current_block = NULL;
    eden_thread->start_block = NULL;
    eden_thread->end_block = NULL;
    return eden_thread;
}

bool belongs_to_eden(struct eden * eden, uintptr_t ptr) {
    return belongs_to_memory_space(&eden->memory_space, ptr);
}

struct eden_block_allocation try_claim_eden_block(struct eden * eden, int n_blocks) {
    uint8_t * actual_current_start = 0;
    uint8_t * next_current_end = 0;

    do {
        actual_current_start = atomic_load(&eden->memory_space.current);
        next_current_end = actual_current_start + (n_blocks * eden->size_blocks_in_bytes);

        if (next_current_end > eden->memory_space.end) {
            return (struct eden_block_allocation) {.success = false};
        }
    } while (!atomic_compare_exchange_strong(&eden->memory_space.current, &actual_current_start, next_current_end));

    return (struct eden_block_allocation) {
        .start_block = (uint8_t *) (uint64_t) actual_current_start,
        .end_block = (uint8_t *) next_current_end,
        .success = true
    };
}

bool can_allocate_object_in_block_eden(struct eden_thread * eden_thread, size_t size_bytes) {
    return eden_thread->start_block != NULL && eden_thread->current_block + size_bytes <= eden_thread->end_block;;
}

struct object * allocate_object_in_block_eden(struct eden_thread * eden_thread, size_t size_bytes) {
    struct object * ptr = (struct object *) eden_thread->current_block;
    eden_thread->current_block += size_bytes;
    return ptr;
}
