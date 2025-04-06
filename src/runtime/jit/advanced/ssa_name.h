#pragma once

#include "shared.h"

//These nodes will be only used when inserting phi functions in the graph ir creation process
#define CREATE_SSA_NAME(local_number, version) (struct ssa_name) {.value = {local_number, version}}
#define CREATE_SSA_NAME_FROM_U64(u64_value) (struct ssa_name) {.u16 = (uint16_t) u64_value}

struct ssa_name {
    union {
        struct {
            uint8_t local_number;
            uint8_t version;
        } value;
        uint16_t u16;
    };
};