#pragma once

struct function_profile_data {

};

struct instruction_profile_data {
    union {
        struct {
            int taken;
            int not_taken;
        } branch;

        struct {
            int left_i64;
            int left_f64;
            int left_string;

            int right_i64;
            int right_f64;
            int right_string;
        } binary_op; //Arithmetic & comparation
    } as;
};