#pragma once

#include "shared.h"
#include "types.h"
#include "shared/types/string_object.h"
#include "runtime/profiler/profile_data.h"


typedef lox_value_t (*native_fn)(int n_args, lox_value_t * args);

#define VOID_NATIVE_RETURN TO_LOX_VALUE_NUMBER(0)

struct native_function_object {
    struct object object;
    native_fn native_fn;
    char * name;
    profile_data_type_t returned_type;
};

struct native_function_object * alloc_native_object(native_fn native_fn, char * name);

#define TO_NATIVE(value) ((struct native_function_object *) AS_OBJECT(value))