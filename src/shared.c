#include "shared.h"

uint32_t hash_string(const char * string_ptr, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= string_ptr[i];
        hash *= 16777619;
    }

    return hash;
}