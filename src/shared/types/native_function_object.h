#pragma once

#include "shared.h"
#include "types.h"
#include "shared/types/string_object.h"
#include "runtime/profiler/profile_data.h"

//Native functin called by vm.c
typedef lox_value_t (*native_vm_fn)(int n_args, lox_value_t * args);
//Native functin called by the JIT compiled code
typedef void * native_jit_fn;

#define VOID_NATIVE_RETURN TO_LOX_VALUE_NUMBER(0)

struct native_function_object {
    struct object object;
    native_vm_fn native_vm_fn;
    native_jit_fn native_jit_fn;
    char * name;
    profile_data_type_t returned_type;
};

struct native_function_object * alloc_native_object(native_vm_fn, native_jit_fn, char * name);

#define TO_NATIVE(value) ((struct native_function_object *) AS_OBJECT(value))