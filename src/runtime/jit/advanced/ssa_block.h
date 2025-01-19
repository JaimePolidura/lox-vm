#pragma once

#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/queue_list.h"
#include "shared/utils/collections/u8_set.h"
#include "ssa_control_node.h"

typedef enum {
    TYPE_NEXT_SSA_BLOCK_SEQ,
    TYPE_NEXT_SSA_BLOCK_LOOP,
    TYPE_NEXT_SSA_BLOCK_BRANCH,
    TYPE_NEXT_SSA_BLOCK_NONE, //OP_RETURN returns won't point to anything
} type_next_ssa_block_t;

#define BELONGS_TO_LOOP_BODY_BLOCK(block) ((block)->nested_loop_body > 0)


struct ssa_block_loop_info {
    struct ssa_block * condition_block;
    struct u8_set modified_local_numbers;
};

//A block represents a list of instructions that run sequentially, there are no branches inside it
//If there is a branch instruction, this block will point to a different block which will contain the branched code
struct ssa_block {
    struct ssa_control_node * first; //Inclusive
    struct ssa_control_node * last; //Inclusive

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
    struct ssa_block * loop_condition_block;
    //Contains global information of all the blocks that belongs to a loop body.
    //Maintained per each loop condition block (is_loop_condition is set to true)
    //Initialized lazily, Reseted (Set to null, when a block is modified using ssa_block methods)
    struct ssa_block_loop_info * loop_info;

    //Set of pointers to ssa_block that points to this ssa_block
    struct u64_set predecesors;

    //First block of ssa ir. This is the only block whose predecessors set is emtpy
    struct ssa_block * ssa_ir_head_block;

    //Set of struct ssa_name, defined in the current block
    struct u64_set defined_ssa_names;

    type_next_ssa_block_t type_next_ssa_block;

    union {
        struct ssa_block * next;
        struct ssa_block * loop;
        struct {
            struct ssa_block * true_branch;
            struct ssa_block * false_branch;
        } branch;
    } next_as;
};

struct ssa_block * alloc_ssa_block(struct lox_allocator *);
void init_ssa_block(struct ssa_block *, struct lox_allocator *);
void free_ssa_block(struct ssa_block *);

void add_last_control_node_ssa_block(struct ssa_block *, struct ssa_control_node *);
void add_before_control_node_ssa_block(struct ssa_block *, struct ssa_control_node * before, struct ssa_control_node * new);
void add_after_control_node_ssa_block(struct ssa_block *, struct ssa_control_node * after, struct ssa_control_node * new);
void remove_control_node_ssa_block(struct ssa_block *, struct ssa_control_node *);
bool is_emtpy_ssa_block(struct ssa_block *);
//a dominates b
bool dominates_ssa_block(struct ssa_block * a, struct ssa_block * b, struct lox_allocator *);
struct u64_set get_dominator_set_ssa_block(struct ssa_block *, struct lox_allocator *);
struct ssa_block_loop_info * get_loop_info_ssa_block(struct ssa_block *, struct lox_allocator *);

typedef bool (*ssa_block_consumer_t)(struct ssa_block *, void *);
enum {
    SSA_BLOCK_OPT_NOT_REPEATED = 1 << 0, //A block can be iterated multiple ways, if many paths leads to that block
    SSA_BLOCK_OPT_REPEATED = 1 << 1, //A block can be called only once
};
//If the callback returns false, the next blocks from the current block won't be scanned.
void for_each_ssa_block(struct ssa_block *, struct lox_allocator *, void * extra, long options, ssa_block_consumer_t);

//Replaces references to old_block of the predecessors of old_block to point to new_block
//Example: A -> B -> C. replace_block_ssa_block(old_block = B, new_block = C), the result: A -> C
void replace_block_ssa_block(struct ssa_block * old_block, struct ssa_block * new_block);
//Removes a true/false branch/block and the subsequent children of the branch/block to remove (subgraph)
struct branch_removed {
    struct u64_set ssa_name_definitions_removed;
    struct u64_set blocks_removed;
};
struct branch_removed remove_branch_ssa_block(struct ssa_block * branch_block, bool true_branch, struct lox_allocator *);

type_next_ssa_block_t get_type_next_ssa_block(struct ssa_control_node *);
