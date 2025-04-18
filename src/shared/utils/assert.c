#include "assert.h"

static void perform_assert_failed(char *, char * error_message, va_list error_message_format);

void lox_assert_failed(char * method_location, char * error_message, ...) {
    va_list args;
    va_start(args, error_message);
    perform_assert_failed(method_location, error_message, args);
}

void lox_assert_false(bool condition_to_be_false, char * method_location,  char * error_message, ...) {
    if (condition_to_be_false) {
        va_list args;
        va_start(args, error_message);
        perform_assert_failed(method_location, error_message, args);
    }
}


void lox_assert(bool condition_to_be_true, char * method_location,  char * error_message, ...) {
    if (!condition_to_be_true) {
        va_list args;
        va_start(args, error_message);
        perform_assert_failed(method_location, error_message, args);
    }
}

static void perform_assert_failed(char * method_location, char * error_message, va_list error_message_format) {
    //TODO Signal stop threads

    fprintf(stderr, "[Failed Runtime assert] at %s: \"", method_location);
    vfprintf(stderr, error_message, error_message_format);
    fprintf(stderr, "\"", method_location);
    va_end(error_message_format);
    fputs("\n", stderr);

    exit(-1);
}