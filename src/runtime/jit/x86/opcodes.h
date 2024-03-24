#pragma once

#include "shared/utils/collections/u8_arraylist.h"
#include "shared.h"
#include "operands.h"

//a <- b
void emit_mov(struct u8_arraylist * array, struct operand a, struct operand b);
void emit_add(struct u8_arraylist * array, struct operand a, struct operand b);
void emit_sub(struct u8_arraylist * array, struct operand a, struct operand b);
void emit_cmp(struct u8_arraylist * array, struct operand a, struct operand b);
void emit_sete_al(struct u8_arraylist * array);
void emit_setg_al(struct u8_arraylist * array);
void emit_setl_al(struct u8_arraylist * array);
void emit_al_movzx(struct u8_arraylist * array, struct operand a);
void emit_or(struct u8_arraylist * array, struct operand a, struct operand b);