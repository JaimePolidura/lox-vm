#pragma once

#include "shared.h"

#define PICK_FIRST_NOT_ZERO(a, b) a == 0 ? b : a
#define PICK_FIRST_NOT_ZERO_3(a, b, c) a == 0 ? (b == 0 ? c : b) : a
#define MAX(a, b) a > b ? a : b

#define TO_UPPER_CASE(character) ( ~((character >= 'a' & character <= 'z') << 5) & character )

#define COMPILER_BARRIER() asm volatile ("" : : : "memory")

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 1)

//It zeros the memory
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*) grow_array(newCount * sizeof(type), pointer, sizeof(type) * (oldCount))

#define VARARGS_TO_ARRAY(type, arr, count, ...) \
do { \
    va_list args; \
    va_start(args, count); \
    for (int i = 0; i < count; ++i) { \
        arr[i] = va_arg(args, type); \
    } \
    va_end(args); \
} while (0)

char * to_upper_case(char * key, int length);
bool string_contains(char * string, int length, char to_check);
bool string_equals_ignore_case(char * a, char * b, int length_a);
uint32_t hash_string(const char * string_ptr, int length);
char * copy_string(char * source, int end);
int string_replace(char * string, int length, char old, char new);
void * grow_array(size_t new_size, void * original_array, size_t old_size);
int round_up_8(int number);
int align(int not_aligned, int value_to_align);
bool has_decimals(double double_value);