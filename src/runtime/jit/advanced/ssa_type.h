#pragma once

#include "shared.h"
#include "shared/utils/collections/u64_hash_table.h"
#include "runtime/profiler/profile_data.h"
#include "shared/types/struct_definition_object.h"

#define CREATE_SSA_TYPE(type_arg, allocator) (create_ssa_type((type_arg), (allocator)))
#define CREATE_STRUCT_SSA_TYPE(definition, allocator) (create_initialize_struct_ssa_type((definition), (allocator)))
#define CREATE_ARRAY_SSA_TYPE(array_type, allocator) (create_array_ssa_type((array_type), (allocator)))
#define IS_NATIVE_SSA_TYPE(type) ((type) >= SSA_TYPE_NATIVE_I64 && (type) <= SSA_TYPE_NATIVE_STRUCT_INSTANCE)
#define IS_STRING_SSA_TYPE(type) ((type) == SSA_TYPE_LOX_STRING || (type) == SSA_TYPE_NATIVE_STRING)

typedef enum {
    SSA_TYPE_F64, //Floating numbers have the same binary representation in lox & native format
    SSA_TYPE_UNKNOWN,

    SSA_TYPE_NATIVE_I64,
    SSA_TYPE_NATIVE_STRING,
    SSA_TYPE_NATIVE_BOOLEAN,
    SSA_TYPE_NATIVE_NIL,
    SSA_TYPE_NATIVE_ARRAY,
    SSA_TYPE_NATIVE_STRUCT_INSTANCE,

    SSA_TYPE_LOX_ANY,
    SSA_TYPE_LOX_I64,
    SSA_TYPE_LOX_STRING,
    SSA_TYPE_LOX_BOOLEAN,
    SSA_TYPE_LOX_NIL,
    SSA_TYPE_LOX_ARRAY,
    SSA_TYPE_LOX_STRUCT_INSTANCE,
} ssa_type_t;

struct struct_instance_ssa_type {
    struct struct_definition_object * definition; //If null, this struct instance can represent any struct
    struct u64_hash_table type_by_field_name; //Mapping of string to struct ssa_type
};

struct array_ssa_type {
    struct ssa_type * type; //If null the array type is unknown
};

struct ssa_type {
    ssa_type_t type;

    union {
        struct struct_instance_ssa_type * struct_instance;
        struct array_ssa_type * array;
    } value;
};

struct ssa_type * create_ssa_type(ssa_type_t, struct lox_allocator *);
struct ssa_type * create_initialize_struct_ssa_type(struct struct_definition_object *, struct lox_allocator *);
struct ssa_type * create_array_ssa_type(struct ssa_type *, struct lox_allocator *);
struct ssa_type * get_struct_field_ssa_type(struct ssa_type *, char *, struct lox_allocator *);

bool is_eq_ssa_type(struct ssa_type *, struct ssa_type *);
ssa_type_t profiled_type_to_ssa_type(profile_data_type_t);

ssa_type_t lox_type_to_native_ssa_type(ssa_type_t);
ssa_type_t native_type_to_lox_ssa_type(ssa_type_t);
char * to_string_ssa_type(ssa_type_t);