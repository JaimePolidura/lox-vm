#pragma once

#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/queue_list.h"
#include "shared/utils/collections/u8_set.h"
#include "lox_ir_control_node.h"

typedef enum {
    TYPE_NEXT_LOX_IR_BLOCK_SEQ,
    TYPE_NEXT_LOX_IR_BLOCK_LOOP,
    TYPE_NEXT_LOX_IR_BLOCK_BRANCH,
    TYPE_NEXT_LOX_IR_BLOCK_NONE, //OP_RETURN returns won't point to anything
} type_next_lox_ir_block_t;

#define BELONGS_TO_LOOP_BODY_BLOCK(block) ((block)->nested_loop_body > 0)

struct lox_ir_block_loop_info {
    struct lox_ir_block * condition_block;
    struct u8_set modified_local_numbers;
};

//A block represents a list of instructions that run sequentially, there are no branches inside it
//If there is a branch instruction, this block will point to a different block which will contain the branched code
struct lox_ir_block {
    struct lox_ir_control_node * first; //Inclusive
    struct lox_ir_control_node * last; //Inclusive

    //Set of local variable numbers that this block will use before assigning
    //Example: i = i + 1; print a; a = 12; b = 1; print b. use_before_assigment: {i, a}
    //This facilitates the insertion of phi functions in loop bodies. So before using a variable, we know what variables to copy.
    //This is used only at jit ir construction time.
    struct u8_set use_before_assigment;

    //Number of nested loops of which the block belongs.
    //If it is 0, it means that the block doest not belong to a loop body
    int nested_loop_body;
    //Indicates if the current block belongs to the jump_to_operand of a loop
    //If this is true, it means that another OP_LOOP control_node_to_lower will point to this control_node_to_lower
    bool is_loop_condition;
    //Every block in a loop body will contain a pointer to the block jump_to_operand.
    //If a control is a loop condtiion (has is_loop_condition set to true), loop_condition_block will point to the outer loop block jump_to_operand.
    struct lox_ir_block * loop_condition_block;
    //Contains global information of all the blocks that belongs to a loop body.
    //Maintained per each loop jump_to_operand block (is_loop_condition is set to true)
    //Initialized lazily, Reseted (Set to null, when a block is modified using lox_ir_block methods)
    struct lox_ir_block_loop_info * loop_info;

    //Set of pointers to lox_ir_block that points to this lox_ir_block
    struct u64_set predecesors;

    //First block of lox ir. This is the only block whose predecessors set is emtpy
    struct lox_ir_block * lox_ir_head_block;

    //Set of struct ssa_name, defined in the current block
    struct u64_set defined_ssa_names;

    type_next_lox_ir_block_t type_next;

    union {
        struct lox_ir_block * next;
        struct lox_ir_block * loop;
        struct {
            struct lox_ir_block * true_branch;
            struct lox_ir_block * false_branch;
        } branch;
    } next_as;
};

struct lox_ir_block * alloc_lox_ir_block(struct lox_allocator *allocator);
void init_lox_ir_block(struct lox_ir_block *block, struct lox_allocator *allocator);

void add_last_control_node_lox_ir_block(struct lox_ir_block *block, struct lox_ir_control_node *node);
void add_first_control_node_lox_ir_block(struct lox_ir_block *block, struct lox_ir_control_node *node);
//If before is NULL, the node will be added as the last node in the block
void add_before_control_node_lox_ir_block(struct lox_ir_block *block, struct lox_ir_control_node * before, struct lox_ir_control_node * new);
//If after is NULL, the node will be added as the first node in the block
void add_after_control_node_lox_ir_block(struct lox_ir_block *block, struct lox_ir_control_node * after, struct lox_ir_control_node * new);
void replace_control_node_lox_ir_block(struct lox_ir_block*, struct lox_ir_control_node* prev, struct lox_ir_control_node* new);
bool is_emtpy_lox_ir_block(struct lox_ir_block *block);
struct lox_ir_block_loop_info * get_loop_info_lox_ir_block(struct lox_ir_block *block, struct lox_allocator *allocator);
//These methods are only used by lox_ir:
void record_removed_node_information_of_block(struct lox_ir_block*,struct lox_ir_control_node*);
void reset_loop_info(struct lox_ir_block*);
//Returns set of pointers to struct lox_ir_block*
struct u64_set get_successors_lox_ir_block(struct lox_ir_block*, struct lox_allocator*);

typedef bool (*lox_ir_block_consumer_t)(struct lox_ir_block *, void *);
enum {
    LOX_IR_BLOCK_OPT_NOT_REPEATED = 1 << 0, //A block can be iterated multiple ways, if many paths leads to that block
    LOX_IR_BLOCK_OPT_REPEATED = 1 << 1, //A block can be called only once
};
//If the callback returns false, the next blocks from the current block won't be scanned.
void for_each_lox_ir_block(struct lox_ir_block*,struct lox_allocator*,void*,long,lox_ir_block_consumer_t);

//Replaces references to old_block of the predecessors of old_block to point to new_block
//Example: A -> B -> C. replace_block_lox_ir_block(old_block = B, new_block = C), the result: A -> C
void replace_block_lox_ir_block(struct lox_ir_block * old_block, struct lox_ir_block * new_block);

type_next_lox_ir_block_t get_type_next_lox_ir_block(struct lox_ir_control_node *node);
