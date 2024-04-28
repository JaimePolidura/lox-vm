#pragma once

#include "shared/utils/collections/u8_arraylist.h"
#include "shared.h"
#include "operands.h"

//a <- b
//These functions returns the beginning index of the emitted instruction. An instruction can occupy multiple bytes
uint16_t emit_mov(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_add(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_sub(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_cmp(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_sete_al(struct u8_arraylist * array);
uint16_t emit_setg_al(struct u8_arraylist * array);
uint16_t emit_setl_al(struct u8_arraylist * array);
uint16_t emit_al_movzx(struct u8_arraylist * array, struct operand a);
uint16_t emit_or(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_and(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_neg(struct u8_arraylist * array, struct operand a);
uint16_t emit_imul(struct u8_arraylist * array, struct operand a, struct operand b);
uint16_t emit_idiv(struct u8_arraylist * array, struct operand divisor);
uint16_t emit_near_jmp(struct u8_arraylist * array, int offset);
uint16_t emit_near_jne(struct u8_arraylist * array, int offset);
uint16_t emit_push(struct u8_arraylist * array, struct operand operand);
uint16_t emit_pop(struct u8_arraylist * array, struct operand operand);
uint16_t emit_call(struct u8_arraylist * array, struct operand operand);
uint16_t emit_ret(struct u8_arraylist * array);
uint16_t emit_nop(struct u8_arraylist * array);
uint16_t emit_inc(struct u8_arraylist * array, struct operand operand);
uint16_t emit_dec(struct u8_arraylist * array, struct operand operand);
uint16_t emit_int(struct u8_arraylist * array, int interrupt_number);