#pragma once

#include "shared/utils/collections/u8_arraylist.h"
#include "shared.h"
#include "operands.h"

/**
 * Byte -> 1 byte
 * Word -> 2 bytes
 * DWord -> 4 bytes
 * QWord -> 8 bytes
 */

#define REX_PREFIX 0x48
//0100
#define FIRST_OPERAND_LARGE_REG_REX_PREFIX 0x01
//0001
#define SECOND_OPERAND_LARGE_REG_REX_PREFIX 0x04

#define IS_BYTE_SIZE(item) item < 128 && item >= -127

#define IS_QWORD_SIZE(item) ((item & 0xFFFFFFFF00000000) != 0)

//11000000
#define REGISTER_ADDRESSING_MODE 0xC0
//01000000
#define BYTE_DISPLACEMENT_MODE 0x40
//10000000
#define DWORD_DISPLACEMENT_MODE 0x80
//00000000
#define ZERO_DISPLACEMENT_MODE 0x00

void emit_prologue_x64_stack(struct u8_arraylist *);
void emit_epilogue_x64_stack(struct u8_arraylist *);

//a <- b
//These functions returns the beginning index of the emitted instruction. An instruction can occupy multiple bytes
uint16_t emit_mov(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_add(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_sub(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_cmp(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_sete_al(struct u8_arraylist * array);
uint16_t emit_setg_al(struct u8_arraylist * array);
uint16_t emit_setl_al(struct u8_arraylist * array);
uint16_t emit_al_movzx(struct u8_arraylist * array, struct lox_ir_ll_operand a);
uint16_t emit_or(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_and(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_neg(struct u8_arraylist * array, struct lox_ir_ll_operand a);
uint16_t emit_imul(struct u8_arraylist * array, struct lox_ir_ll_operand a, struct lox_ir_ll_operand b);
uint16_t emit_idiv(struct u8_arraylist * array, struct lox_ir_ll_operand divisor);
uint16_t emit_near_jmp(struct u8_arraylist * array, int offset);
uint16_t emit_near_jne(struct u8_arraylist * array, int offset);
uint16_t emit_push(struct u8_arraylist * array, struct lox_ir_ll_operand operand);
uint16_t emit_pop(struct u8_arraylist * array, struct lox_ir_ll_operand operand);
uint16_t emit_call(struct u8_arraylist * array, struct lox_ir_ll_operand operand);
uint16_t emit_ret(struct u8_arraylist * array);
uint16_t emit_nop(struct u8_arraylist * array);
uint16_t emit_inc(struct u8_arraylist * array, struct lox_ir_ll_operand operand);
uint16_t emit_dec(struct u8_arraylist * array, struct lox_ir_ll_operand operand);
uint16_t emit_int(struct u8_arraylist * array, int interrupt_number);