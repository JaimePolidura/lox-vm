#include "lox_allocator.h"

struct native_allocator native_lox_allocator = (struct native_allocator) {
        .lox_allocator = (struct lox_allocator) {
            .lox_malloc = native_lox_malloc,
            .lox_free = native_lox_free,
        }
};

void * native_lox_malloc(struct lox_allocator *, size_t size) {
    return malloc(size);
}

void native_lox_free(struct lox_allocator *, void * ptr) {
    free(ptr);
}