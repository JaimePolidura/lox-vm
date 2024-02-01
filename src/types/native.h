#pragma once

#include "shared.h"
#include "types.h"

typedef lox_value_t (*native_fn)(int n_args, lox_value_t * args);

struct native_object {
    struct object object;
    native_fn native_fn;
};

struct native_object * alloc_native_object(native_fn native_fn);

#define TO_NATIVE(value) ((struct native_object *) AS_OBJECT(value))
