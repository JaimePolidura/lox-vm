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
    PROFILE_DATA_TYPE_ARRAY,
    PROFILE_DATA_TYPE_STRUCT_INSTANCE,
    PROFILE_DATA_TYPE_ANY
} profile_data_type_t;

struct function_profile_data {
    struct instruction_profile_data * profile_by_instruction_index;
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
    int n_arguments;
    struct type_profile_data * arguments_type_profile;
};

struct instruction_profile_data {
    union {
        struct {
            int taken;
            int not_taken;
        } branch;

        struct {
            //Records in definition the struct definition of the operation.
            //If initialized is true and definition is NULL, it means that the struct definition could be any type
            struct struct_definition_object * definition;
            bool initialized;
        } struct_field;

        //A callee will profile the type of the returned value, and the arguments type passed by the caller
        //A pointer of the caller struct function_call_profile_data is passed to the callee
        //Parallel calls won't be profiled
        struct function_call_profile function_call;
    } as;
};


profile_data_type_t lox_value_to_profile_type(lox_value_t value);
char * profile_data_type_to_string(profile_data_type_t);

void init_function_profile_data(struct function_profile_data *, int n_instructions, int n_locals);

struct instruction_profile_data * alloc_instruction_profile_data();
void init_instruction_profile_data(struct instruction_profile_data *);
void init_type_profile_data(struct type_profile_data *);
