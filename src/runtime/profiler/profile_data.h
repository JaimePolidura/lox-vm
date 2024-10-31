#pragma once

#include "shared.h"

struct function_profile_data {
    struct instruction_profile_data * data_by_instruction_index;
};

struct binary_node_profile_data {
    int i64;
    int f64;
    int string;
};

struct instruction_profile_data {
    union {
        struct {
            int taken;
            int not_taken;
        } branch;

        struct {
            struct binary_node_profile_data left;
            struct binary_node_profile_data right;
        } binary_op; //Arithmetic & comparation
    } as;
};

void init_function_profile_data(struct function_profile_data *, int n_instructions);

struct instruction_profile_data * alloc_instruction_profile_data();
void init_instruction_profile_data(struct instruction_profile_data *instruction_profile_data);