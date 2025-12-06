#pragma once

#include "runtime/jit/advanced/lox_ir.h"

//Low level lox ir lowerer
struct lllil {
    //Key: Block pointer Value: Pointer to struct ptr_arraylist
    //The ptr_arraylist will contain the size of the stack slots. For example: [8, 64, 8] Theree stack slots
    //The first one will occupy from 0 to 8, the second one from 8 to 72 and the third one from 72 to 80.
    struct u64_hash_table stack_slots_by_block;
    //Contains set of pointers to processed blocks. Useful to iterate blocks whose predecessors have been scanned
    struct u64_set processed_blocks;
    int last_phi_resolution_v_reg_allocated;

    struct lox_ir * lox_ir;
    struct arena_lox_allocator lllil_allocator;
};

struct lllil_control {
    struct lllil * lllil;
    struct lox_ir_control_node * control_node_to_lower;
    struct lox_ir_control_node * last_node_lowered;
};

struct lllil * alloc_low_level_lox_ir_lowerer(struct lox_ir *);
void free_low_level_lox_ir_lowerer(struct lllil *);

//Will return index to stack slot
uint16_t allocate_stack_slot_lllil(struct lllil*, struct lox_ir_block*, size_t);

void add_lowered_node_lllil_control(struct lllil_control*, struct lox_ir_control_node*);