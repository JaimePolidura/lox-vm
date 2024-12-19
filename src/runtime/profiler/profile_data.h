#pragma once

#include "shared.h"
#include "shared/types/types.h"
#include "shared/types/string_object.h"
#include "shared/bytecode/bytecode.h"

typedef enum {
    PROFILE_DATA_TYPE_I64,
    PROFILE_DATA_TYPE_F64,
    PROFILE_DATA_TYPE_STRING,
    PROFILE_DATA_TYPE_BOOLEAN,
    PROFILE_DATA_TYPE_NIL,
    PROFILE_DATA_TYPE_ARRAY,
    PROFILE_DATA_TYPE_STRUCT_INSTANCE,
    PROFILE_DATA_TYPE_ANY
} profile_data_type_t;

struct function_profile_data {
    struct instruction_profile_data * profile_by_instruction_index;

    int n_arguments;
    struct type_profile_data * arguments_type_profile;
};

struct type_profile_data {
    int i64;
    int f64;
    int boolean;
    int nil;
    int string;
    int array;
    int struct_instance;
    int any; //Any other type
};

struct function_call_profile {
    struct type_profile_data returned_type_profile;
};

struct instruction_profile_data {
    union {
        struct {
            int taken;
            int not_taken;
        } branch;

        struct {
            //Records in struct_definition the struct struct_definition of the operation.
            //If initialized is true and struct_definition is NULL, it means that the struct struct_definition could be any type
            struct struct_definition_object * definition;
            bool initialized;
            struct type_profile_data get_struct_field_profile;
        } struct_field;

        struct type_profile_data get_array_element_profile;

        //A callee will profile the type of the returned value, and the arguments type passed by the caller
        //A pointer of the caller struct function_call_profile_data is passed to the callee
        //Parallel calls won't be profiled
        struct function_call_profile function_call;
    } as;
};

extern struct function_object;
void init_function_profile_data(struct function_object *);

struct instruction_profile_data * alloc_instruction_profile_data();
void init_instruction_profile_data(struct instruction_profile_data *);
void init_type_profile_data(struct type_profile_data *);

profile_data_type_t lox_value_to_profile_type(lox_value_t value);
char * profile_data_type_to_string(profile_data_type_t);
profile_data_type_t get_type_by_type_profile_data(struct type_profile_data type_profile);
bool can_true_branch_be_discarded_instruction_profile_data(struct instruction_profile_data);
bool can_false_branch_be_discarded_instruction_profile_data(struct instruction_profile_data);
