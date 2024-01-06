#include "types.h"

bool cast_to_boolean(lox_value_t value) {
    return value.as.number != 0;
}