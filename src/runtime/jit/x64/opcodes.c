#include "opcodes.h"

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

#define IS_BYTE_SIZE(item) item < 128 || item >= -127

#define IS_QWORD_SIZE(item) ((item & 0xFFFFFFFF00000000) != 0)

//11000000
#define REGISTER_ADDRESSING_MODE 0xC0
//01000000
#define BYTE_DISPLACEMENT_MODE 0x40
//10000000
#define DWORD_DISPLACEMENT_MODE 0x80
//00000000
#define ZERO_DISPLACEMENT_MODE 0x00

static uint8_t get_64_bit_binary_op_prefix(struct operand a, struct operand b);
static uint8_t get_16_bit_prefix();

static uint16_t emit_register_disp_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_to_disp_register_move(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_immediate_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b);
static void emit_dword_immediate_value(struct u8_arraylist * array, uint64_t immediate_value);
static void emit_qword_immediate_value(struct u8_arraylist * array, uint64_t immediate_value);
static uint16_t emit_register_register_add(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_immediate_add(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_register_sub(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_immediate_sub(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_register_cmp(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_immediate_cmp(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_register_binary_or(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_immediate_binary_or(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_register_imul(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_immediate_imul(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_push_pop(struct u8_arraylist * array, struct operand a, uint8_t base_opcode);
static uint16_t emit_register_register_binary_and(struct u8_arraylist * array, struct operand a, struct operand b);
static uint16_t emit_register_immediate_binary_and(struct u8_arraylist * array, struct operand a, struct operand b);

uint16_t emit_dec(struct u8_arraylist * array, struct operand operand) {
    uint16_t instruction_index = append_u8_arraylist(array, operand.as.reg >= R8 ? 0x49 : 0x48);
    append_u8_arraylist(array, 0xFF);
    append_u8_arraylist(array, 0xc8 + TO_32_BIT_REGISTER(operand.as.reg));

    return instruction_index;
}

uint16_t emit_inc(struct u8_arraylist * array, struct operand operand) {
    uint16_t instruction_index = append_u8_arraylist(array, operand.as.reg >= R8 ? 0x49 : 0x48);
    append_u8_arraylist(array, 0xFF);
    append_u8_arraylist(array, 0xc0 + TO_32_BIT_REGISTER(operand.as.reg));

    return instruction_index;
}

uint16_t emit_nop(struct u8_arraylist * array) {
    return append_u8_arraylist(array, 0x90); //Opcode
}

uint16_t emit_call(struct u8_arraylist * array, struct operand operand) {
    bool is_extended_register = operand.as.reg >= R8;

    uint16_t instruction_index = array->in_use;

    if(is_extended_register){
        append_u8_arraylist(array, 0x41); //Prefix
    }

    append_u8_arraylist(array, 0xFF); //Opcode
    append_u8_arraylist(array, 0xD0 + TO_32_BIT_REGISTER(operand.as.reg)); //Opcode

    return instruction_index;
}

uint16_t emit_push(struct u8_arraylist * array, struct operand operand) {
    return emit_push_pop(array, operand, 0x50);
}

uint16_t emit_ret(struct u8_arraylist * array) {
    return append_u8_arraylist(array, 0xc3);
}

uint16_t emit_pop(struct u8_arraylist * array, struct operand operand) {
    return emit_push_pop(array, operand, 0x58);
}

static uint16_t emit_push_pop(struct u8_arraylist * array, struct operand operand, uint8_t base_opcode) {
    bool is_extended_register = operand.as.reg >= R8;

    if(is_extended_register) {
        operand.as.reg -= 8;
    }

    uint16_t instruction_index = 0;
    uint8_t opcode = base_opcode + operand.as.reg;

    if(is_extended_register){
        instruction_index = append_u8_arraylist(array, 0x41); //Prefix
        append_u8_arraylist(array, opcode); //Opcode
    } else {
        instruction_index = append_u8_arraylist(array,  opcode); //Opcode
    }

    return instruction_index;
}

uint16_t emit_near_jne(struct u8_arraylist * array, int offset) {
    uint8_t opcode_1 = 0x0F;
    uint8_t opcode_2 = 0x85;

    uint8_t instruction_offset = append_u8_arraylist(array, opcode_1);
    append_u8_arraylist(array, opcode_2);

    emit_dword_immediate_value(array, offset);

    return instruction_offset;
}

uint16_t emit_near_jmp(struct u8_arraylist * array, int offset) {
    uint8_t opcode = 0xE9; // Short jump or near jump

    uint16_t instruction_offset = append_u8_arraylist(array, opcode);

    emit_dword_immediate_value(array, offset); //Offset plus instruction size (5)

    return instruction_offset;
}

uint16_t emit_idiv(struct u8_arraylist * array, struct operand divisor) {
    uint16_t index = append_u8_arraylist(array, divisor.as.reg >= R8 ? 0x49 : 0x48); //Prefix
    append_u8_arraylist(array, 0xF7); //Opcode
    append_u8_arraylist(array, REGISTER_ADDRESSING_MODE | (0x07 << 3) | TO_32_BIT_REGISTER(divisor.as.reg));

    return index;
}

uint16_t emit_imul(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_register_imul(array, a, b);
    } else {
        emit_register_immediate_imul(array, a, b);
    }

    //TODO Unreachable, panic
    return -1;
}

uint16_t emit_neg(struct u8_arraylist * array, struct operand a) {
    uint16_t offset = append_u8_arraylist(array, a.as.reg >= R8 ? 0x49 : 0x48);
    append_u8_arraylist(array, 0xF7); //Opcode
    append_u8_arraylist(array, (0xD8 + TO_32_BIT_REGISTER(a.as.reg)));

    return offset;
}

uint16_t emit_or(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        return emit_register_register_binary_or(array, a, b);
    } else {
        return emit_register_immediate_binary_or(array, a, b);
    }
}

uint16_t emit_and(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        return emit_register_register_binary_and(array, a, b);
    } else {
        return emit_register_immediate_binary_and(array, a, b);
    }
}

uint16_t emit_al_movzx(struct u8_arraylist * array, struct operand a) {
    uint8_t prefix = a.as.reg >= R8 ? 0x4C : 0x48;
    uint8_t opcode_1 = 0x0F;
    uint8_t opcode_2 = 0xB6;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = 0; //al register
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t offset = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode_1);
    append_u8_arraylist(array, opcode_2);
    append_u8_arraylist(array, mod_reg_rm);

    return offset;
}

uint16_t emit_setl_al(struct u8_arraylist * array) {
    uint16_t index = append_u8_arraylist(array, get_16_bit_prefix());
    append_u8_arraylist(array, 0x9C); //Opcode
    append_u8_arraylist(array, 0xC0); //ModRM

    return index;
}

uint16_t emit_setg_al(struct u8_arraylist * array) {
    uint16_t index = append_u8_arraylist(array, get_16_bit_prefix());
    append_u8_arraylist(array, 0x9F); //Opcode
    append_u8_arraylist(array, 0xC0); //ModRM

    return index;
}

uint16_t emit_sete_al(struct u8_arraylist * array) {
    uint16_t index = append_u8_arraylist(array, get_16_bit_prefix());
    append_u8_arraylist(array, 0x94); //Opcode
    append_u8_arraylist(array, 0xC0); //ModRM

    return index;
}

uint16_t emit_cmp(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        return emit_register_register_cmp(array, a, b);
    } else {
        return emit_register_immediate_cmp(array, a, b);
    }

    return -1; //TODO Unreachable, panic
}

uint16_t emit_add(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND){
        emit_register_register_add(array, a, b);
    } else if(a.type == REGISTER_OPERAND && b.type == IMMEDIATE_OPERAND){
        emit_register_immediate_add(array, a, b);
    }

    return -1;
}

uint16_t emit_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND) {
        return emit_register_to_register_mov(array, a, b); //mov a, b
    } else if(a.type == REGISTER_OPERAND && b.type == IMMEDIATE_OPERAND){
        return emit_immediate_to_register_mov(array, a, b); //mov a, immediate b
    } else if(a.type == REGISTER_OPERAND && b.type == REGISTER_DISP_OPERAND) {
        return emit_register_disp_to_register_mov(array, a, b); //mov a, displacement b
    } else if(a.type == REGISTER_DISP_OPERAND && b.type == REGISTER_OPERAND){
        return emit_register_to_disp_register_move(array, a, b); //mov displacement a, b
    }

    return -1;
}

uint16_t emit_sub(struct u8_arraylist * array, struct operand a, struct operand b) {
    if(a.type == REGISTER_OPERAND && b.type == REGISTER_OPERAND) {
        return emit_register_register_sub(array, a, b);
    } else if(a.type == REGISTER_OPERAND && b.type == IMMEDIATE_OPERAND) {
        return emit_register_immediate_sub(array, a, b);
    }

    return -1;
}

static uint16_t emit_register_immediate_imul(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x69;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);
    emit_dword_immediate_value(array, b.as.immediate);

    return index;
}

static uint16_t emit_register_register_imul(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode_1 = 0x0F;
    uint8_t opcode_2 = 0xAF;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode_1);
    append_u8_arraylist(array, opcode_2);
    append_u8_arraylist(array, mod_reg_rm);

    return index;
}

static uint16_t emit_register_register_binary_or(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x09;
    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(b.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);

    return index;
}

static uint16_t emit_register_immediate_binary_and(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = a.as.reg >= R8 ? 0x49 : 0x48;
    uint8_t opcode = b.as.immediate >= 128 ? (a.as.reg == RAX ? 0x25 : 0x81) : 0x83;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(b.as.immediate < 128 || a.as.reg != RAX) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0x01 << 5;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mode_reg_rm = mode | reg | rm;

        append_u8_arraylist(array, mode_reg_rm);
    }

    if(b.as.immediate >= 128){
        emit_dword_immediate_value(array, b.as.immediate);
    } else {
        append_u8_arraylist(array, b.as.immediate);
    }

    return index;
}

static uint16_t emit_register_immediate_binary_or(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = a.as.reg >= R8 ? 0x49 : 0x48;
    uint8_t opcode = b.as.immediate >= 128 ? (a.as.reg == RAX ? 0x0d : 0x81) : 0x83;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(b.as.immediate < 128 || a.as.reg != RAX) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0x01 << 3;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mode_reg_rm = mode | reg | rm;

        append_u8_arraylist(array, mode_reg_rm);
    }

    if(b.as.immediate >= 128){
        emit_dword_immediate_value(array, b.as.immediate);
    } else {
        append_u8_arraylist(array, b.as.immediate);
    }

    return index;
}

static uint16_t emit_register_register_binary_and(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = a.as.reg >= R8 ?
            (b.as.reg >= R8 ? 0x4d : 0x49) :
            (b.as.reg >= R8 ? 0x4c : 0x48);

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, 0x21);

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(b.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mode_reg_rm = mode | reg | rm;

    append_u8_arraylist(array, mode_reg_rm);

    return index;
}

static uint16_t emit_register_immediate_cmp(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = a.as.reg == RAX && b.as.immediate >= 128 ? 0x3D : 0x81;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(a.as.reg != RAX) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0x30;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mod_reg_rm = mode | reg | rm;

        append_u8_arraylist(array, mod_reg_rm);
    }

    emit_dword_immediate_value(array, b.as.immediate);

    return index;
}

static uint16_t emit_register_register_cmp(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x39;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(a.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(b.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);

    return index;
}

static uint16_t emit_register_register_sub(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x29;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(b.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);

    return index;
}

static uint16_t emit_register_immediate_sub(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = a.as.reg >= R8 ? 0x49 : 0x48;
    uint8_t opcode = b.as.immediate >= 128 ? (a.as.reg == RAX ? 0x2d : 0x81) : 0x83;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(a.as.reg != RAX || b.as.immediate < 128){
        append_u8_arraylist(array, 0xe8 + TO_32_BIT_REGISTER(a.as.reg));
    }
    
    if(b.as.immediate >= 128) {
        emit_dword_immediate_value(array, b.as.immediate);
    } else {
        append_u8_arraylist(array, b.as.immediate);
    }

    return index;
}

static uint16_t emit_register_immediate_add(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = a.as.reg >= R9 ? 0x49 : 0x48;
    uint8_t opcode = b.as.immediate >= 128 ? (a.as.reg == RAX ? 0x05 : 0x81) : 0x83;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);

    if(a.as.reg != RAX || b.as.immediate <= 128) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mod_reg_rm = mode | reg | rm;

        append_u8_arraylist(array, mod_reg_rm);
    }

    if(b.as.immediate >= 128){
        emit_dword_immediate_value(array, b.as.immediate);
    } else {
        append_u8_arraylist(array, b.as.immediate);
    }


    return index;
}

static uint16_t emit_register_register_add(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x01;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(b.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);

    return index;
}

static uint16_t emit_immediate_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    bool is_immediate_64_bit = (b.as.immediate & 0xFFFFFFFF00000000) != 0;

    uint16_t index = append_u8_arraylist(array, a.as.reg >= R8 ? 0x49 : 0x48); //Prefix
    append_u8_arraylist(array, is_immediate_64_bit ? (0xB8 + TO_32_BIT_REGISTER(a.as.reg)) : 0xC7); //Opcode

    if (!is_immediate_64_bit) {
        uint8_t mode = REGISTER_ADDRESSING_MODE;
        uint8_t reg = 0;
        uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
        uint8_t mod_reg_rm = mode | reg | rm;
        append_u8_arraylist(array, mod_reg_rm);
    }

    if(is_immediate_64_bit){
        emit_qword_immediate_value(array, b.as.immediate);
    } else {
        emit_dword_immediate_value(array, b.as.immediate);
    }

    return index;
}

static uint16_t emit_register_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    uint8_t prefix = get_64_bit_binary_op_prefix(a, b);
    uint8_t opcode = 0x89;

    uint8_t mode = REGISTER_ADDRESSING_MODE;
    uint8_t reg = TO_32_BIT_REGISTER(b.as.reg) << 3;
    uint8_t rm = TO_32_BIT_REGISTER(a.as.reg);
    uint8_t mod_reg_rm = mode | reg | rm;

    uint16_t index = append_u8_arraylist(array, prefix);
    append_u8_arraylist(array, opcode);
    append_u8_arraylist(array, mod_reg_rm);

    return index;
}

//mov [a +- 8], b
static uint16_t emit_register_to_disp_register_move(struct u8_arraylist * array, struct operand a, struct operand b) {
    register_t a_reg = a.as.reg_disp.reg;
    int disp = a.as.reg_disp.disp;
    register_t b_reg = b.as.reg;

    //Prefix
    uint16_t instruction_index = append_u8_arraylist(array, b_reg < R8 ?
                                                            (a_reg < R8 ? 0x48 : 0x49) :
                                                            (a_reg < R8 ? 0x4c : 0x4d));

    append_u8_arraylist(array, 0x89); //Opcode

    uint8_t mode = IS_BYTE_SIZE(a.as.reg_disp.disp) ?
            BYTE_DISPLACEMENT_MODE :
            DWORD_DISPLACEMENT_MODE;
    append_u8_arraylist(array, mode | TO_32_BIT_REGISTER(b_reg) << 3 | TO_32_BIT_REGISTER(a_reg));

    if(TO_32_BIT_REGISTER(a_reg) == RSP) {
        append_u8_arraylist(array, 0x24);
    }
    if(TO_32_BIT_REGISTER(a_reg) == RBP && mode == ZERO_DISPLACEMENT_MODE) {
        append_u8_arraylist(array, 0x00);
    }

    if(mode == BYTE_DISPLACEMENT_MODE){
        append_u8_arraylist(array, disp);
    } else if(mode == DWORD_DISPLACEMENT_MODE){
        emit_dword_immediate_value(array, disp);
    }

    return instruction_index;
}

//mov a, [b +- 8]
static uint16_t emit_register_disp_to_register_mov(struct u8_arraylist * array, struct operand a, struct operand b) {
    register_t b_reg = b.as.reg_disp.reg;
    register_t a_reg = a.as.reg;
    int disp = b.as.reg_disp.disp;

    //Prefix
    uint16_t instruction_index = append_u8_arraylist(array, a_reg >= R8 ?
        (b_reg >= R8 ? 0x4d : 0x4c) :
        (b_reg >= R8 ? 0x49 : 0x48));

    append_u8_arraylist(array, 0x8b); //Opcode

    uint8_t mode = IS_BYTE_SIZE(b.as.reg_disp.disp) ? BYTE_DISPLACEMENT_MODE : DWORD_DISPLACEMENT_MODE;
    append_u8_arraylist(array, mode | TO_32_BIT_REGISTER(a_reg) << 3 | TO_32_BIT_REGISTER(b_reg));

    if(TO_32_BIT_REGISTER(b_reg) == RSP) {
        append_u8_arraylist(array, 0x24);
    }

    IS_BYTE_SIZE(b.as.reg_disp.disp) ?
        append_u8_arraylist(array, disp) :
        emit_dword_immediate_value(array, disp);

    return instruction_index;
}

static uint8_t get_64_bit_binary_op_prefix(struct operand a, struct operand b) {
    uint8_t prefix = REX_PREFIX; // 0x48
    if(a.type == REGISTER_OPERAND && a.as.reg >= R8)
        prefix |= FIRST_OPERAND_LARGE_REG_REX_PREFIX; //Final result: 0x4c
    if(a.type == REGISTER_DISP_OPERAND && a.as.reg_disp.reg >= R8)
        prefix |= FIRST_OPERAND_LARGE_REG_REX_PREFIX; //Final result: 0x4c

    if(b.type == REGISTER_OPERAND && b.as.reg >= R8)
        prefix |= SECOND_OPERAND_LARGE_REG_REX_PREFIX; //Final result if prev condition false: 0x49, if true: 0x4d
    if(b.type == REGISTER_DISP_OPERAND && b.as.reg_disp.reg >= R8)
        prefix |= SECOND_OPERAND_LARGE_REG_REX_PREFIX; //Final result if prev condition false: 0x49, if true: 0x4d

    return prefix;
}

static uint8_t get_16_bit_prefix() {
    return 0X0F;
}

static void emit_dword_immediate_value(struct u8_arraylist * array, uint64_t immediate_value){
    append_u8_arraylist(array, (immediate_value & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 8) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 16) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 24) & 0xFF));
}

static void emit_qword_immediate_value(struct u8_arraylist * array, uint64_t immediate_value){
    emit_dword_immediate_value(array, immediate_value);

    append_u8_arraylist(array, ((immediate_value >> 32) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 40) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 48) & 0xFF));
    append_u8_arraylist(array, ((immediate_value >> 56) & 0xFF));
}
