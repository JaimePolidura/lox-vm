#pragma once

#include "shared.h"
#include "registers.h"

#define IMMEDIATE_TO_OPERAND(immediate) ((struct operand) {IMMEDIATE_OPERAND, immediate})
#define REGISTER_TO_OPERAND(reg) ((struct operand) {REGISTER_OPERAND, reg})

struct operand {
    enum {
        IMMEDIATE_OPERAND,
        REGISTER_OPERAND
    } type;

    union {
        uint64_t immediate;
        register_t reg;
    } as;
};