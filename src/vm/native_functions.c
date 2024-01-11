#include "native_functions.h"

lox_value_t clock_native(int n_args, lox_value_t * args) {
    return FROM_RAW_TO_NUMBER(1000000); //TODO Example
}