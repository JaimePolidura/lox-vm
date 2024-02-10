#include "substring.h"

int length_substring(struct substring substring) {
    return substring.end_exclusive - substring.start_inclusive;
}

char * start_substring(struct substring substring) {
    return substring.source + substring.start_inclusive;
}

char * copy_substring_to_string(struct substring substring) {
    int length = length_substring(substring);
    char * ptr = malloc(sizeof(char) * length + 1);
    memcpy(ptr, start_substring(substring), length);
    ptr[length] = '\x00';
    return ptr;
}