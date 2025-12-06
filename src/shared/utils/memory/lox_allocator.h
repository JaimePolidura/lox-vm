#pragma once

#include "shared.h"

struct lox_allocator {
    void* (*lox_malloc)(struct lox_allocator*, size_t);
    void (*lox_free)(struct lox_allocator*, void *);
};

struct native_allocator {
    struct lox_allocator lox_allocator;
};

void * native_lox_malloc(struct lox_allocator *, size_t size);
void native_lox_free(struct lox_allocator *, void * ptr);

extern struct native_allocator native_lox_allocator;

#define LOX_MALLOC(allocator, size) ((allocator)->lox_malloc((allocator), (size)))
#define LOX_FREE(allocator, ptr) (allocator->lox_free(allocator, ptr))
#define NATIVE_LOX_ALLOCATOR() (&native_lox_allocator.lox_allocator)
#define NATIVE_LOX_MALLOC(size) (NATIVE_LOX_ALLOCATOR()->lox_malloc(NATIVE_LOX_ALLOCATOR(), (size)))
#define NATIVE_LOX_FREE(ptr) (NATIVE_LOX_ALLOCATOR()->lox_free(NATIVE_LOX_ALLOCATOR(), (ptr)))