#include "utils.h"

char * to_upper_case(char * src, int length) {
    char * new = malloc(sizeof(char) * (length + 1));
    for(int i = 0; i < length; i++){
        new[i] = TO_UPPER_CASE(src[i]);
    }
    new[length] = 0x00;

    return new;
}

bool string_contains(char * string, int length, char to_check) {
    for(int i = 0; i < length; i++){
        if(string[i] == to_check){
            return true;
        }
    }

    return false;
}

bool string_equals_ignore_case(char * a, char * b, int length_a) {
    for(int i = 0; i < length_a; i++){
        if(TO_UPPER_CASE(a[i]) != TO_UPPER_CASE(b[i])){
            return false;
        }
    }

    return true;
}

uint32_t hash_string(const char * string_ptr, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= string_ptr[i];
        hash *= 16777619;
    }

    return hash;
}

char * copy_string(char * source, int length) {
    char * dst = malloc(sizeof(char) * (length + 1));
    for(int i = 0; i < length; i++){
        dst[i] = source[i];
    }
    dst[length] = 0x00;

    return dst;
}

int string_replace(char * string, int length, char old, char new) {
    int n_replaced = 0;
    for(int i = 0; i < length; i++){
        if(string[i] == old){
            string[i] = new;
            n_replaced++;
        }
    }

    return n_replaced;
}

int round_down_power_two(int number) {
    if (number == 0) {
        return 0;
    }

    number |= (number >> 1);
    number |= (number >> 2);
    number |= (number >> 4);
    number |= (number >> 8);
    number |= (number >> 16);

    return number - (number >> 1);
}

int round_up_8(int number) {
    return ceil(number / 8.0) * 8;
}

int align(int not_aligned, int value_to_align) {
    return ((not_aligned + value_to_align - 1) / value_to_align) * value_to_align;
}

bool has_decimals(double double_value) {
    return double_value != floor(double_value);
}

bool is_double_power_of_2(double doule_value) {
    if (doule_value <= 0 || has_decimals(doule_value)) {
        return false;
    }
    uint64_t u64_value = (uint64_t) doule_value;
    return (u64_value & (u64_value - 1)) == 0;
}

void write_u16_le(void * ptr, uint16_t value) {
    *((uint16_t *) ptr) = value;
}

uint16_t read_u16_le(void * ptr) {
    return *((uint16_t *) ptr);
}

//Given: 27, this will return:
// powers: 4, 3
// extra: 3
// 2^4 + 2^3 + 3 = 27
struct decomposed_power_two decompose_power_of_two(int number_to_decompose) {
    int * exponents = NATIVE_LOX_MALLOC(sizeof(int) * 32);
    int remaining_value = number_to_decompose;
    int n_exponents = 0;

    while (remaining_value > 0) {
        int largest_power_of_two = round_down_power_two(remaining_value);
        int exponent = log2(largest_power_of_two);

        exponents[n_exponents++] = exponent;
        remaining_value -= largest_power_of_two;
    }

    return (struct decomposed_power_two) {
        .n_exponents = n_exponents,
        .remainder = remaining_value,
        .exponents = exponents,
    };
}