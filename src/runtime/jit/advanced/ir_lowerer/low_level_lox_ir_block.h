#pragma once

#include "low_level_lox_ir_node.h"

struct lox_level_lox_ir_block {
    struct lox_level_lox_ir_node first;
    struct lox_level_lox_ir_node last;

    bool is_seq_next;

    union {
        struct lox_level_lox_ir_block * next;
        struct {
            struct lox_level_lox_ir_block * true_branch;
            struct lox_level_lox_ir_block * false_branch;
        } branch;
    } next_as;
};