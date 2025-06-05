#pragma once

#include "runtime/jit/advanced/lox_ir.h"

struct liverange {
    int v_reg_ssa_name_local_number;

    //If from_control_node and from_block is NULL, it means from the liverange will go just from the start of IR
    //For example this could happen when we used function arguments
    struct lox_ir_block * from_block;
    struct lox_ir_control_ll_move * from_control_node;
    int from_control_node_index;

    struct lox_ir_block * to_block;
    struct lox_ir_control_node * to_control_node;
    int to_control_node_index;
};

void perform_live_range_analysis(struct lox_ir *);