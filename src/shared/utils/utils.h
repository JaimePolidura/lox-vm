#pragma once

#include "shared.h"
#include "shared/utils/memory/lox_allocator.h"

#define PICK_FIRST_NOT_ZERO(a, b) a == 0 ? b : a
#define PICK_FIRST_NOT_ZERO_3(a, b, c) a == 0 ? (b == 0 ? c : b) : a
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define IS_FLAG_SET(to_test, flag) (((to_test) & (flag)) == flag)
#define TO_UPPER_CASE(character) ( ~((character >= 'a' & character <= 'z') << 5) & character )
#define COMPILER_BARRIER() asm volatile ("" : : : "memory")
#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 1)
//It zeros the memory
#define GROW_ARRAY(allocator, type, pointer, oldCount, newCount) \
    (type*) grow_array(allocator, newCount * sizeof(type), pointer, sizeof(type) * (oldCount))
#define VARARGS_TO_ARRAY(type, arr, count, ...) \
do { \
    va_list args; \
    va_start(args, count); \
    for (int i = 0; i < count; ++i) { \
        arr[i] = va_arg(args, type); \
    } \
    va_end(args); \
} while (0)
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)((char *)__mptr - offsetof(type,member));})

//Given: 27, this will return:
// powers: 4, 3
// extra: 3
// 2^4 + 2^3 + 3 = 27
//Powers is heap allocated
struct decomposed_power_two {
    int * exponents;
    int n_exponents;
    int remainder;
};
struct decomposed_power_two decompose_power_of_two(int number_to_decompose);

char * to_upper_case(char * key, int length);
bool string_contains(char * string, int length, char to_check);
bool string_equals_ignore_case(char * a, char * b, int length_a);
uint32_t hash_string(const char * string_ptr, int length);
char * copy_string(char * source, int end);
int string_replace(char * string, int length, char old, char new);
void * grow_array(struct lox_allocator *, size_t new_size, void * original_array, size_t old_size);
int round_up_8(int number);
int round_down_power_two(int number);
int align(int not_aligned, int value_to_align);
bool has_decimals(double double_value);
char * dynamic_format_string(const char * format, ...);
bool is_double_power_of_2(double);
void write_u16_le(void *, uint16_t);
uint16_t read_u16_le(void *);
