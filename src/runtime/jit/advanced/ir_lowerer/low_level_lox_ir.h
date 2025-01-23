#pragma once

#include "shared.h"
#include "runtime/jit/advanced/phi_resolution/v_register.h"
#include "shared/bytecode/bytecode.h"

typedef enum {
    LOW_LEVEL_LOX_IR_OPERAND_REGISTER,
    LOW_LEVEL_LOX_IR_OPERAND_IMMEDIATE,
    LOW_LEVEL_LOX_IR_OPERAND_MEMORY_ADDRESS,
} low_level_lox_ir_operand_type_type;

struct operand {
    union {
        struct v_register v_register;
        uint64_t immedate;
        struct {
            struct operand * address;
            uint64_t offset;
        } memory_address;
    };

    low_level_lox_ir_operand_type_type type;
};

typedef enum {
    LOW_LEVEL_LOX_IR_NODE_MOVE,
    LOW_LEVEL_LOX_IR_NODE_BINARY,
    LOW_LEVEL_LOX_IR_NODE_UNARY,
    LOW_LEVEL_LOX_IR_NODE_FUNCTION_EXIT,
    LOW_LEVEL_LOX_IR_NODE_FUNCTION_CALL,
    LOW_LEVEL_LOX_IR_NODE_JUMP,
    LOW_LEVEL_LOX_IR_NODE_GROW_STACK,
    LOW_LEVEL_LOX_IR_NODE_SHRINK_STACK,
} low_level_lox_ir_control_node_type;

struct lox_level_lox_ir_node {
    low_level_lox_ir_control_node_type type;
};

struct low_level_lox_ir_move {
    struct lox_level_lox_ir_node node;
    struct operand from;
    struct operand to;
};

struct low_level_lox_ir_binary {
    struct lox_level_lox_ir_node node;
    struct operand left;
    struct operand right;
    bytecode_t operator;
};

struct low_level_lox_ir_unary {
    struct lox_level_lox_ir_node node;
    struct operand operand;
    bytecode_t operator;
};

struct low_level_lox_ir_jump {
    struct lox_level_lox_ir_node node;
    struct operand jump_to;
};

struct low_level_lox_ir_grow_stack {
    struct lox_level_lox_ir_node node;
    uint64_t to_grow;
};

struct low_level_lox_ir_shrink_stack {
    struct lox_level_lox_ir_node node;
    uint64_t to_shrink;
};