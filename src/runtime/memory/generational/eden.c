#include "eden.h"

struct eden * alloc_eden(struct config config) {
    size_t size_blocks_in_bytes = config.generational_gc_config.eden_block_size_kb * 1024;
    size_t size_eden_in_bytes = config.generational_gc_config.eden_size_mb * 1024 * 1024;

    struct eden * eden = malloc(sizeof(struct eden));
    int n_addresses = (int) round_up_8(size_eden_in_bytes / 8);
    eden->updated_references_mark_bitmap = alloc_mark_bitmap(n_addresses, (uint64_t) eden->memory_space.start);
    eden->moved_mark_bitmap = alloc_mark_bitmap(n_addresses, (uint64_t) eden->memory_space.start);
    init_memory_space(&eden->memory_space, size_eden_in_bytes);
    eden->size_blocks_in_bytes = size_blocks_in_bytes;

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
    uint64_t actual_current = 0;
    uint64_t new_current = 0;

    do {
        actual_current = (uint64_t) eden->memory_space.current;
        new_current = (uint64_t) actual_current + (n_blocks * eden->size_blocks_in_bytes);

        if(new_current > (uint64_t) eden->memory_space.end){
            return (struct eden_block_allocation) {.success = false};
        }
    }while(!atomic_compare_exchange_strong(eden->memory_space.current, (uint8_t *) &actual_current, new_current));

    return (struct eden_block_allocation) {
        .start_block = (uint8_t *) (uint64_t) new_current - (n_blocks * eden->size_blocks_in_bytes),
        .end_block = (uint8_t *) new_current,
        .success = true
    };
}

bool can_allocate_object_in_block_eden(struct eden_thread * eden_thread, size_t size_bytes) {
    return eden_thread->start_block != NULL && eden_thread->current_block + size_bytes <= eden_thread->end_block;
}

struct object * allocate_object_in_block_eden(struct eden_thread * eden_thread, size_t size_bytes) {
    struct object * ptr = (struct object *) eden_thread->current_block;
    eden_thread->current_block += size_bytes;
    return ptr;
}