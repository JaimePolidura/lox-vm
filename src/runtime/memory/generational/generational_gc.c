#include "generational_gc.h"

extern __thread struct vm_thread * self_thread;
extern struct config config;
extern struct vm current_vm;
extern void start_minor_generational_gc();

static struct object * try_alloc_object(size_t size);
static void try_claim_eden_block_or_start_gc(size_t size_bytes);
static void save_new_eden_block_info(struct eden_block_allocation);

struct struct_instance_object * __attribute__((weak)) alloc_struct_instance_gc_alg(struct struct_definition_object * definition) {
    struct struct_instance_object * instance = (struct struct_instance_object *) try_alloc_object(sizeof(struct struct_instance_object));
    init_struct_instance_object(instance, definition);
    return instance;
}

struct string_object * __attribute__((weak)) alloc_string_gc_alg(char * chars, int length) {
    //TODO Add chars into eden space
    struct string_object * string = (struct string_object *) try_alloc_object(sizeof(struct string_object));
    init_object(&string->object, OBJ_STRING);
    string->length = length;
    string->hash = hash_string(chars, length);
    string->chars = malloc(sizeof(char) * length + 1);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    return string;
}

struct array_object * __attribute__((weak)) alloc_array_gc_alg(int n_elements) {
    int n_elements_rounded_up = round_up_8(n_elements);
    struct array_object * array = (struct array_object *) try_alloc_object(sizeof(struct array_object) + (n_elements_rounded_up * sizeof(lox_value_t)));
    init_object(&array->object, OBJ_ARRAY);
    array->values.values = (lox_value_t *) array + sizeof(struct array_object);
    array->values.capacity = n_elements;
    array->values.in_use = 0;

    return array;
}

void * __attribute__((weak)) alloc_gc_object_info_alg() {
    return NULL;
}

void * __attribute__((weak)) alloc_gc_thread_info_alg() {
    struct generational_thread_gc * generational_gc = malloc(sizeof(struct generational_thread_gc));
    generational_gc->eden = alloc_eden_thread();
    return generational_gc;
}

void * __attribute__((weak)) alloc_gc_vm_info_alg() {
    struct generational_gc * generational_gc = malloc(sizeof(struct generational_gc));
    generational_gc->survivor = alloc_survivor(config);
    generational_gc->eden = alloc_eden(config);
    generational_gc->old = alloc_old(config);
    return generational_gc;
}

struct gc_result __attribute__((weak)) try_start_gc_alg() {
    start_minor_generational_gc();

    return (struct gc_result) {
            .bytes_allocated_before_gc = 0,
            .bytes_allocated_after_gc = 0,
    };
}

static struct object * try_alloc_object(size_t size_bytes) {
    struct generational_thread_gc * gc_thread_info = self_thread->gc_info;
    struct eden_thread * eden_thread_info = gc_thread_info->eden;

    if (!can_allocate_object_in_block_eden(eden_thread_info, size_bytes)) {
        try_claim_eden_block_or_start_gc(size_bytes);
    }

    return allocate_object_in_block_eden(eden_thread_info, size_bytes);
}

static void try_claim_eden_block_or_start_gc(size_t size_bytes) {
    struct generational_gc * global_gc_info = current_vm.gc;
    int n_blocks = (int) ceil(size_bytes / config.generational_gc_config.eden_block_size_kb * 1024);
    struct eden_block_allocation block_allocation = try_claim_eden_block(global_gc_info->eden, n_blocks);

    if (!block_allocation.success) {
        start_minor_generational_gc();
        block_allocation = try_claim_eden_block(global_gc_info->eden, n_blocks);
    }

    save_new_eden_block_info(block_allocation);
}

static void save_new_eden_block_info(struct eden_block_allocation eden_block_allocation) {
    struct generational_thread_gc * gc_thread_info = self_thread->gc_info;
    struct eden_thread * eden_thread_info = gc_thread_info->eden;

    eden_thread_info->start_block = eden_block_allocation.start_block;
    eden_thread_info->current_block = eden_block_allocation.start_block;
    eden_thread_info->end_block = eden_block_allocation.end_block;
}