#pragma once

#include "shared.h"

struct lox_allocator {
    void *(*lox_malloc)(struct lox_allocator*, size_t);
    void (*lox_free)(struct lox_allocator*, void *);
};

void * native_lox_malloc(struct lox_allocator *, size_t size);
void native_lox_free(struct lox_allocator *, void * ptr);

#define NATIVE_LOX_ALLOCATOR() (struct native_allocator) { \
    .lox_allocator = (struct lox_allocator) {.lox_free = default_lox_free, .lox_malloc = default_lox_malloc} \
}

struct native_allocator {
    struct lox_allocator lox_allocator;
};

void * native_lox_malloc(struct lox_allocator *, size_t size) {
    return malloc(size);
}

void native_lox_free(struct lox_allocator *, void * ptr) {
    free(ptr);
}