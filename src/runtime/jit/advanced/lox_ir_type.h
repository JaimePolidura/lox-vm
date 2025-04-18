#pragma once

#include "runtime/profiler/profile_data.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_set.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/array_object.h"
#include "shared/utils/assert.h"
#include "shared.h"

#define CREATE_LOX_IR_TYPE(type_arg, allocator) (create_lox_ir_type((type_arg), (allocator)))
#define CREATE_STRUCT_LOX_IR_TYPE(definition, allocator) (create_initialize_struct_lox_ir_type((definition), (allocator)))
#define CREATE_ARRAY_LOX_IR_TYPE(array_type, allocator) (create_array_lox_ir_type((array_type), (allocator)))
#define IS_I64_LOX_IR_TYPE(type) ((type) == LOX_IR_TYPE_LOX_I64 || (type) == LOX_IR_TYPE_NATIVE_I64)
#define IS_ARRAY_LOX_IR_TYPE(type) ((type) == LOX_IR_TYPE_NATIVE_ARRAY || (type) == LOX_IR_TYPE_LOX_ARRAY)
#define IS_STRING_LOX_IR_TYPE(type) ((type) == LOX_IR_TYPE_NATIVE_STRING || (type) == LOX_IR_TYPE_LOX_STRING)
#define IS_STRUCT_LOX_IR_TYPE(type) ((type) == LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE || (type) == LOX_IR_TYPE_LOX_STRUCT_INSTANCE)
#define IS_NIL_LOX_IR_TYPE(type) ((type) == LOX_IR_TYPE_LOX_NIL || (type) == LOX_IR_TYPE_NATIVE_NIL)

//Dont change order
typedef enum {
    LOX_IR_TYPE_F64, //Floating numbers have the same binary representation in lox & native format
    LOX_IR_TYPE_UNKNOWN,

    LOX_IR_TYPE_NATIVE_I64,
    LOX_IR_TYPE_NATIVE_STRING,
    LOX_IR_TYPE_NATIVE_BOOLEAN,
    LOX_IR_TYPE_NATIVE_NIL,
    LOX_IR_TYPE_NATIVE_ARRAY,
    LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE,

    LOX_IR_TYPE_LOX_ANY,
    LOX_IR_TYPE_LOX_I64,
    LOX_IR_TYPE_LOX_STRING,
    LOX_IR_TYPE_LOX_BOOLEAN,
    LOX_IR_TYPE_LOX_NIL,
    LOX_IR_TYPE_LOX_ARRAY,
    LOX_IR_TYPE_LOX_STRUCT_INSTANCE,

    //It should always be the last element, this is usefule to know how many types there are
    LOX_IR_TYPE_LOX_LAST_TYPE
} lox_ir_type_t;

struct struct_instance_lox_ir_type {
    struct struct_definition_object * definition; //If null, this struct instance can represent any struct
    struct u64_hash_table type_by_field_name; //Mapping of string to struct lox_ir_type
};

struct array_lox_ir_type {
    struct lox_ir_type * type; //If null the array type is unknown
};

struct lox_ir_type {
    lox_ir_type_t type;

    union {
        struct struct_instance_lox_ir_type * struct_instance;
        struct array_lox_ir_type * array;
    } value;
};

struct lox_ir_type * create_lox_ir_type(lox_ir_type_t, struct lox_allocator *);
struct lox_ir_type * create_initialize_struct_lox_ir_type(struct struct_definition_object *, struct lox_allocator *);
struct lox_ir_type * create_array_lox_ir_type(struct lox_ir_type *, struct lox_allocator *);
struct lox_ir_type * clone_lox_ir_type(struct lox_ir_type *, struct lox_allocator *);

struct lox_ir_type * get_struct_field_lox_ir_type(struct lox_ir_type *struct_type, char *field_name, struct lox_allocator *allocator);
bool is_eq_lox_ir_type(struct lox_ir_type *a, struct lox_ir_type *b);
struct lox_ir_type * native_to_lox_lox_ir_type(struct lox_ir_type *native_type, struct lox_allocator *allocator);
struct lox_ir_type * lox_to_native_lox_ir_type(struct lox_ir_type *lox_type, struct lox_allocator *allocator);

#define RETURN_LOX_TYPE_AS_DEFAULT true
#define RETURN_NATIVE_TYPE_AS_DEFAULT true
//Only takes inputs as lox types and returns lox type, not native types
lox_ir_type_t binary_to_lox_ir_type(bytecode_t operator, lox_ir_type_t left, lox_ir_type_t right);
lox_ir_type_t profiled_type_to_lox_ir_type(profile_data_type_t profiled_type);
bool is_lox_lox_ir_type(lox_ir_type_t type);
bool is_native_lox_ir_type(lox_ir_type_t type);
lox_ir_type_t lox_type_to_native_lox_ir_type(lox_ir_type_t lox_type);
lox_ir_type_t native_type_to_lox_ir_type(lox_ir_type_t native_type);
char * to_string_lox_ir_type(lox_ir_type_t type);
bool is_same_format_lox_ir_type(lox_ir_type_t left, lox_ir_type_t right);
bool is_format_equivalent_lox_ir_type(lox_ir_type_t left, lox_ir_type_t right);
bool is_same_number_binay_format_lox_ir_type(lox_ir_type_t left, lox_ir_type_t right);
uint64_t value_lox_to_native_lox_ir_type(lox_value_t lox_value);
lox_value_t value_native_to_lox_ir_type(uint64_t native_value, lox_ir_type_t expected_lox_type);