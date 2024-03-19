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