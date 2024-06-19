#pragma once

#define INCREMENT_GENERATION(object) \
    do {                                 \
        uint8_t new_generation = GET_GENERATION(object) + 1; \
        uint8_t * forwading_ptr = GET_FORWARDING_PTR(object);                         \
        object->gc_info = (void *) (((uint64_t) forwading_ptr) | ((uint64_t) (new_generation << 61)));  \
    } while(false);
#define GET_GENERATION(object_ptr) ((uint8_t) object_ptr->gc_info >> 61)

#define SET_FORWARDING_PTR(object_ptr, new_ptr) (object_ptr)->gc_info = (void *) ((uint64_t) (new_ptr) | ((uint64_t) object->gc_info & 0xE000000000000000))
#define GET_FORWARDING_PTR(object_ptr) ((uint8_t *) ((uint64_t) object->gc_info & 0x1FFFFFFFFFFFFFFF))