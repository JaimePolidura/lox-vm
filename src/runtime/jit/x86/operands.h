#pragma once

#include "shared.h"
#include "registers.h"

#define IMMEDIATE_TO_OPERAND(immediate) ((struct operand) {IMMEDIATE_OPERAND, immediate})
#define REGISTER_TO_OPERAND(reg) ((struct operand) {REGISTER_OPERAND, reg})
#define DISPLACEMENT_TO_OPERAND(reg, disp) ((struct operand) {.type = REGISTER_DISP_OPERAND, .as = { .reg_disp = {reg, disp} }})

#define RAX_OPERAND REGISTER_TO_OPERAND(RAX)
#define RCX_OPERAND REGISTER_TO_OPERAND(RCX)
#define RDX_OPERAND REGISTER_TO_OPERAND(RDX)
#define RBX_OPERAND REGISTER_TO_OPERAND(RBX)
#define RSP_OPERAND REGISTER_TO_OPERAND(RSP)
#define RBP_OPERAND REGISTER_TO_OPERAND(RBP)
#define RSI_OPERAND REGISTER_TO_OPERAND(RSI)
#define RDI_OPERAND REGISTER_TO_OPERAND(RDI)
#define R8_OPERAND REGISTER_TO_OPERAND(R8)
#define R9_OPERAND REGISTER_TO_OPERAND(R9)
#define R10_OPERAND REGISTER_TO_OPERAND(R10)
#define R11_OPERAND REGISTER_TO_OPERAND(R11)
#define R12_OPERAND REGISTER_TO_OPERAND(R12)
#define R13_OPERAND REGISTER_TO_OPERAND(R13)
#define R14_OPERAND REGISTER_TO_OPERAND(R14)
#define R15_OPERAND REGISTER_TO_OPERAND(R15)

struct operand {
    enum {
        IMMEDIATE_OPERAND,
        REGISTER_OPERAND,
        REGISTER_DISP_OPERAND //Linear displacement, exmpl: mov eax,-8[rpb]
    } type;

    union {
        uint64_t immediate;
        register_t reg;
        struct { //Linear displacement, exmpl: mov eax,-8[rpb]
            register_t reg;
            int disp;
        } reg_disp;
    } as;
};