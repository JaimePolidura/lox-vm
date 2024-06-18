#include "generational_gc.h"

extern struct config config;

void * alloc_gc_thread_info_alg() {
    struct generational_thread_gc * generational_gc = malloc(sizeof(struct generational_thread_gc));
    generational_gc->eden = alloc_eden_thread();
    return generational_gc;
}

void * alloc_gc_alg() {
    struct generational_gc * generational_gc = malloc(sizeof(struct generational_gc));
    generational_gc->survivor = alloc_survivor(config);
    generational_gc->eden = alloc_eden(config);
    generational_gc->old = alloc_old(config);
    return generational_gc;
}