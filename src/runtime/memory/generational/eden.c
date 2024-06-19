#include "eden.h"

struct eden * alloc_eden(struct config config) {
    size_t size_in_bytes = config.generational_gc_config.eden_block_size_kb * 1024;
    uint8_t * eden_ptr = malloc(size_in_bytes);

    struct eden * eden = malloc(sizeof(struct eden));
    eden->size_blocks_in_bytes = size_in_bytes;
    eden->current = eden_ptr;
    eden->start = eden_ptr;
    eden->end = eden_ptr + size_in_bytes;

    eden->mark_bitmap = alloc_mark_bitmap((int) round_up_8(size_in_bytes / 8), (uint64_t) eden->start);

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
    return ((uintptr_t) eden->start) <= ptr && ((uintptr_t) eden->end) > ptr;
}

struct eden_block_allocation try_claim_eden_block(struct eden * eden, int n_blocks) {
    uint64_t actual_current = 0;
    uint64_t new_current = 0;

    do {
        actual_current = (uint64_t) eden->current;
        new_current = (uint64_t) actual_current + (n_blocks * eden->size_blocks_in_bytes);

        if(new_current > (uint64_t) eden->end){
            return (struct eden_block_allocation) {.success = false};
        }
    }while(!atomic_compare_exchange_strong(eden->current, (uint8_t *) &actual_current, new_current));

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