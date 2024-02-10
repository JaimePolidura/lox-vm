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