#include "arena.h"

static void create_new_block(struct arena *arena);
static inline size_t available_block_size(struct arena_block block);
static inline void * malloc_block(struct arena_block *block, size_t to_allocate);

extern void runtime_panic(char * format, ...);

void init_arena(struct arena * arena) {
    arena->current = NULL;
}

void free_arena(struct arena * arena) {
    struct arena_block * current_arena_block = arena->current;
    while(current_arena_block != NULL){
        struct arena_block * current_arena_to_free = current_arena_block;
        current_arena_block = current_arena_block->prev;
        free(current_arena_to_free);
    }
}

void * malloc_arena(struct arena * arena, size_t to_alloc_size) {
    if(arena->current == NULL){
        create_new_block(arena);
    }
    if(to_alloc_size > ARENA_CONTENT_BLOCK_SIZE){
        runtime_panic("Cannot %i in arena of max to_alloc_size %i", ARENA_CONTENT_BLOCK_SIZE, to_alloc_size);
    }
    if(arena->current->next_index >= ARENA_CONTENT_BLOCK_SIZE || available_block_size(*arena->current) < to_alloc_size){
        create_new_block(arena);
    }

    return malloc_block(arena->current, to_alloc_size);
}

static void create_new_block(struct arena * arena) {
    struct arena_block * new_block = malloc(sizeof(struct arena_block));
    new_block->prev = arena->current;
    arena->current = new_block;
    new_block->next_index = 0;
}

static inline size_t available_block_size(struct arena_block block) {
    return ARENA_CONTENT_BLOCK_SIZE - block.next_index;
}

static inline void * malloc_block(struct arena_block * block, size_t to_allocate) {
    void * ptr = &block->bytes[block->next_index];
    block->next_index += to_allocate;
    block->next_index = round_up_8(block->next_index); //Align 8 bytes
    return ptr;
}

void * fixed_arena_lox_malloc(struct lox_allocator * allocator, size_t bytes) {
    struct arena_lox_allocator * fixed_arena_lox_allocator = (struct arena_lox_allocator *) allocator;
    return malloc_arena(&fixed_arena_lox_allocator->arena, bytes);
}

void fixed_arena_lox_free(struct lox_allocator * allocator, void * ptr) {
    //We don't do anything. The memory will get free when the user manually frees the arena with: "free_arena()"
}

struct arena_lox_allocator * alloc_lox_allocator_arena(struct arena arena, struct lox_allocator * allocator) {
    struct arena_lox_allocator * arena_lox_allocator = LOX_MALLOC(allocator, sizeof(struct arena_lox_allocator));
    arena_lox_allocator->arena = arena;
    arena_lox_allocator->lox_allocator.lox_malloc = fixed_arena_lox_malloc;
    arena_lox_allocator->lox_allocator.lox_free = fixed_arena_lox_free;

    return arena_lox_allocator;
}

struct arena_lox_allocator to_lox_allocator_arena(struct arena arena) {
    return (struct arena_lox_allocator) {
        .arena = arena,
        .lox_allocator = (struct lox_allocator) {
            .lox_malloc = &fixed_arena_lox_malloc,
            .lox_free = &fixed_arena_lox_free
        }
    };
}