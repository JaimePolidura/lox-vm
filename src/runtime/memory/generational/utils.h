#pragma once

#include "shared/types/types.h"
#include "shared/types/struct_instance_object.h"
#include "shared/types/string_object.h"
#include "shared/types/array_object.h"

#define INCREMENT_GENERATION(object) \
    do {                                 \
        uint8_t new_generation = GET_GENERATION(object) + 1; \
        lox_value_t forwading_ptr = ((uint64_t) (GET_FORWARDING_PTR(object))) & 0x1FFFFFFFFFFFFFFF; \
        object->gc_info = (void *) (void *) (forwading_ptr | ((uint64_t) new_generation) << 61); \
    } while(false);
#define GET_GENERATION(object_ptr) ((uint8_t) (((uint64_t) object_ptr->gc_info) >> 61))

#define SET_FORWARDING_PTR(object_ptr, new_ptr) (object_ptr)->gc_info = (void *) ( ( ((uint64_t) (new_ptr)) & 0x1FFFFFFFFFFFFFFF) | ((uint64_t) object_ptr->gc_info & 0xE000000000000000))
#define CLEAR_FORWARDING_PTR(object_ptr) ((object_ptr)->gc_info = (void *) ((uint64_t) (object_ptr)->gc_info & 0xE000000000000000))
#define GET_FORWARDING_PTR(object_ptr) ((lox_value_t) (((uint64_t) (object_ptr)->gc_info & 0x1FFFFFFFFFFFFFFF)) | 0xE000000000000000)
#define IS_CLEARED_FORWARDING_PTR(object_ptr) (((uint64_t) (object_ptr)->gc_info) << 3 == 0)

int get_n_bytes_allocated_object(struct object * object);