#pragma once

#include "opcodes.h"

/*
0100 1101 mov r10, r8
0100 1000 mov 0xff, rax
0100 1001 mov 0xf	f, r8
0100 1100 mov r10, rax
*/
#define REX_PREFIX 0x48
//0100
#define FIRST_OPERAND_LARGE_REG_REX_PREFIX 0x04
//0001
#define SECOND_OPERAND_LARGE_REG_REX_PREFIX 0x01

//11000000
#define REGISTER_ADDRESSING_MODE 0xC0

static void emit_register_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_immediate_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b);
static uint8_t get_64_bit_binary_op_prefix(struct operand a, struct operand b);
static uint8_t get_16_bit_prefix();
static void emit_immediate_value(struct u8_arraylist * array, uint64_t immediate_value);
static void emit_register_register_add(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_immediate_add(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_register_sub(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_immediate_sub(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_register_cmp(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_immediate_cmp(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_register_binary_or(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_immediate_binary_or(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_register_imul(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_register_immediate_imul(struct u8_arraylist * array, struct operand a, struct operand b);

void emit_idiv(struct u8_arraylist * array, struct operand divisor) {
    append_u8_arraylist(array, divisor.as.reg >= R8 ? 0x49 : 0x48); //Prefix
    append_u8_arraylist(array, 0xF7); //Opcode
    append_u8_arraylist(array, REGISTER_ADDRESSING_MODE | (0x07 << 3) | TO_32_BIT_REGISTER(divisor.as.reg));
}

void emit_imul(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_register_imul(array, a, b);
    } else {
        emit_register_immediate_imul(array, a, b);
    }
}

void emit_neg(struct u8_arraylist * array, struct operand a) {
    append_u8_arraylist(array, a.as.reg >= R8 ? 0x49 : 0x48);
    append_u8_arraylist(array, 0xF7); //Opcode
    append_u8_arraylist(array, REGISTER_ADDRESSING_MODE | (0x03 << 3) | TO_32_BIT_REGISTER(a.as.reg)); //ModRM
}

void emit_or(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_register_binary_or(array, a, b);
    } else {
        emit_register_immediate_binary_or(array, a, b);
    }
}

void emit_al_movzx(struct u8_arraylist * array, struct operand a) {
    uint8_t prefix = a.as.reg >= R8 ? 0x4C : 0x48;
    uint8_t opcode_1 = 0x0F;
    uint8_t opcode_2 = 0xB6;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = 0; //al register
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode_1);
    append_u8_arraylist(array, opcode_2);
    append_u8_arraylist(array, mod_reg_rm);
}

void emit_setl_al(struct u8_arraylist * array) {
    append_u8_arraylist(array, get_16_bit_prefix());
    append_u8_arraylist(array, 0x9C); //Opcode
    append_u8_arraylist(array, 0xC0); //ModRM
}

void emit_setg_al(struct u8_arraylist * array) {
    append_u8_arraylist(array, get_16_bit_prefix());
    append_u8_arraylist(array, 0x9F); //Opcode
    append_u8_arraylist(array, 0xC0); //ModRM
}

void emit_sete_al(struct u8_arraylist * array) {
    append_u8_arraylist(array, get_16_bit_prefix());
    append_u8_arraylist(array, 0x94); //Opcode
    append_u8_arraylist(array, 0xC0); //ModRM
}

void emit_cmp(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_register_cmp(array, a, b);
    } else {
        emit_register_immediate_cmp(array, a, b);
    }
}

void emit_add(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_register_add(array, a, b);
    } else if(a.type == REGISTER_OPERAND && b.type == IMMEDIATE_OPERAND){
        emit_register_immediate_add(array, a, b);
    }
}

void emit_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_to_register_mov(array, a, b);
    } else if(a.type == REGISTER_OPERAND && b.type == IMMEDIATE_OPERAND){
        emit_immediate_to_register_mov(array, a, b);
    }
}

void emit_sub(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND) {
        emit_register_register_sub(array, a, b);
    } else if(a.type == REGISTER_OPERAND && b.type == IMMEDIATE_OPERAND) {
        emit_register_immediate_sub(array, a, b);
    }
}

static void emit_register_immediate_imul(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x69;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
    emit_immediate_value(array, b.as.immediate);
}

static void emit_register_register_imul(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode_1 = 0x0F;
    uint8_t opcode_2 = 0xAF;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode_1);
    append_u8_arraylist(array, opcode_2);
    append_u8_arraylist(array, mod_reg_rm);
}

static void emit_register_register_binary_or(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x09;
    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
}

static void emit_register_immediate_binary_or(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x81;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = 0x01 << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mode_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mode_reg_rm);
    emit_immediate_value(array, b.as.immediate);
}

static void emit_register_immediate_cmp(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = a.as.reg == RAX && b.as.immediate >= 128 ? 0x3D : 0x81;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(a.as.reg != RAX) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0x30;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mod_reg_rm = mode | reg | rm;

        append_u8_arraylist(array, mod_reg_rm);
    }

    emit_immediate_value(array, b.as.immediate);
}

static void emit_register_register_cmp(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x39;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
}

static void emit_register_register_sub(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x29;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
}

static void emit_register_immediate_sub(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = a.as.reg == RAX && b.as.immediate >= 128 ? 0x2D : 0x81;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(a.as.reg != RAX){
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0x20;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mod_reg_rm = mode | reg | rm;
        append_u8_arraylist(array, mod_reg_rm);
    }

    emit_immediate_value(array, b.as.immediate);
}

static void emit_register_immediate_add(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = a.as.reg == RAX && b.as.immediate >= 128 ? 0x05 : 0x81;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(a.as.reg != RAX) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mod_reg_rm = mode | reg | rm;

        append_u8_arraylist(array, mod_reg_rm);
    }

    emit_immediate_value(array, b.as.immediate);
}

static void emit_register_register_add(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x01;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
}

static void emit_immediate_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    bool is_immediate_64_bit = (b.as.immediate & 0xFFFFFFFF00000000) != 0;

    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = is_immediate_64_bit ? 0xBC : 0xC7;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = 0;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
    emit_immediate_value(array, b.as.immediate);
}

static void emit_register_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x89;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
}

static uint8_t get_64_bit_binary_op_prefix(struct operand a, struct operand b) {
    uint8_t prefix = REX_PREFIX; // 0x48
    if(a.type == REGISTER_OPERAND && a.as.reg >= R8)
        prefix |= FIRST_OPERAND_LARGE_REG_REX_PREFIX; //Final result: 0x4C
    if(b.type == REGISTER_OPERAND && b.as.reg >= R8)
        prefix |= SECOND_OPERAND_LARGE_REG_REX_PREFIX; //Final result if prev condition false: 0x49, if true: 0x4D

    return prefix;
}

static uint8_t get_16_bit_prefix() {
    return 0X0F;
}

/**
 * 0x000000FF
 *
 * 0xFF000000
 */
static void emit_immediate_value(struct u8_arraylist * array, uint64_t immediate_value) {
    uint8_t max_bytes = (immediate_value & 0xFFFFFFFF00000000) != 0 ? 8 : 4;

    append_u8_arraylist(array, (immediate_value & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 8) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 16) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 24) & 0xFF));

    if(max_bytes > 4){
        append_u8_arraylist(array, ((immediate_value >> 32) & 0xFF));
        append_u8_arraylist(array, ((immediate_value >> 40) & 0xFF));
        append_u8_arraylist(array, ((immediate_value >> 48) & 0xFF));
        append_u8_arraylist(array, ((immediate_value >> 56) & 0xFF));
    }
}