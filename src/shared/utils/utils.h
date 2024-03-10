#pragma once

#include "shared.h"

#define TO_UPPER_CASE(character) ( ~((character >= 'a' & character <= 'z') << 5) & character )

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 1)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)realloc(pointer, sizeof(type) * (newCount))

char * to_upper_case(char * key, int length);
bool string_contains(char * string, int length, char to_check);
bool string_equals_ignore_case(char * a, char * b, int length_a);
uint32_t hash_string(const char * string_ptr, int length);
