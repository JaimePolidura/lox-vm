#pragma once

#include "shared.h"
#include "shared/types/types.h"
#include "shared/types/string_object.h"

typedef enum {
    PROFILE_DATA_TYPE_I64,
    PROFILE_DATA_TYPE_F64,
    PROFILE_DATA_TYPE_STRING,
    PROFILE_DATA_TYPE_BOOLEAN,
    PROFILE_DATA_TYPE_NIL,
    PROFILE_DATA_TYPE_OBJECT,
    PROFILE_DATA_TYPE_ANY
} profile_data_type_t;

struct function_profile_data {
    struct instruction_profile_data * data_by_instruction_index;
    struct type_profile_data * local_profile_data;
};

struct type_profile_data {
    int i64;
    int f64;
    int string;
    int boolean;
    int nil;
    int object;
};

struct instruction_profile_data {
    union {
        struct {
            int taken;
            int not_taken;
        } branch;

        struct {
            struct type_profile_data left;
            struct type_profile_data right;
        } binary_op; //Arithmetic & comparation

        struct { //OP_SET_LOCAL
            struct type_profile_data data;
        } set_local_op;

        struct {
            struct type_profile_data type;
        } struct_field;

    } as;
};

profile_data_type_t lox_value_to_profile_type(lox_value_t value);

profile_data_type_t get_type_by_local_function_profile_data(struct function_profile_data *, int local_number);

void init_function_profile_data(struct function_profile_data *, int n_instructions, int n_locals);

struct instruction_profile_data * alloc_instruction_profile_data();
void init_instruction_profile_data(struct instruction_profile_data *);

void init_type_profile_data(struct type_profile_data *);
