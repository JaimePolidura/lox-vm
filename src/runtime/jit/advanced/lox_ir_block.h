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
    //This is used only at ssa ir construction time.
    struct u8_set use_before_assigment;

    //Number of nested loops of which the block belongs.
    //If it is 0, it means that the block doest not belong to a loop body
    int nested_loop_body;
    //Indicates if the current block belongs to the condition of a loop
    //If this is true, it means that another OP_LOOP control_node will point to this control_node
    bool is_loop_condition;
    //Every block in a loop body will contain a pointer to the block condition.
    //If a node is a loop condtiion (has is_loop_condition set to true), loop_condition_block will point to the outer loop block condition.
    struct lox_ir_block * loop_condition_block;
    //Contains global information of all the blocks that belongs to a loop body.
    //Maintained per each loop condition block (is_loop_condition is set to true)
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
void add_before_control_node_lox_ir_block(struct lox_ir_block *block, struct lox_ir_control_node * before, struct lox_ir_control_node * new);
void add_after_control_node_lox_ir_block(struct lox_ir_block *block, struct lox_ir_control_node * after, struct lox_ir_control_node * new);
void remove_control_node_lox_ir_block(struct lox_ir_block *lox_ir_block, struct lox_ir_control_node *node_to_remove);
bool is_emtpy_ssa_block(struct lox_ir_block *);
//a dominates b
bool dominates_lox_ir_block(struct lox_ir_block * a, struct lox_ir_block * b, struct lox_allocator *allocator);
struct u64_set get_dominator_set_lox_ir_block(struct lox_ir_block *block, struct lox_allocator *allocator);
struct lox_ir_block_loop_info * get_loop_info_lox_ir_block(struct lox_ir_block *block, struct lox_allocator *allocator);

typedef bool (*lox_ir_block_consumer_t)(struct lox_ir_block *, void *);
enum {
    LOX_IR_BLOCK_OPT_NOT_REPEATED = 1 << 0, //A block can be iterated multiple ways, if many paths leads to that block
    LOX_IR_BLOCK_OPT_REPEATED = 1 << 1, //A block can be called only once
};
//If the callback returns false, the next blocks from the current block won't be scanned.
void for_each_lox_ir_block(struct lox_ir_block *start_block, struct lox_allocator *allocator, void * extra, long options, lox_ir_block_consumer_t consumer);

//Replaces references to old_block of the predecessors of old_block to point to new_block
//Example: A -> B -> C. replace_block_lox_ir_block(old_block = B, new_block = C), the result: A -> C
void replace_block_lox_ir_block(struct lox_ir_block * old_block, struct lox_ir_block * new_block);
//Removes a true/false branch/block and the subsequent children of the branch/block to remove (subgraph)
struct branch_removed {
    struct u64_set ssa_name_definitions_removed;
    struct u64_set blocks_removed;
};
struct branch_removed remove_branch_lox_ir_block(struct lox_ir_block * branch_block, bool true_branch, struct lox_allocator *lox_allocator);

type_next_lox_ir_block_t get_type_next_lox_ir_block(struct lox_ir_control_node *node);
