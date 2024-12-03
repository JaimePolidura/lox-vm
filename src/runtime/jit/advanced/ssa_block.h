#pragma once

#include "shared/utils/collections/queue_list.h"1
#include "shared/utils/collections/u8_set.h"
#include "ssa_control_node.h"

typedef enum {
    TYPE_NEXT_SSA_BLOCK_SEQ,
    TYPE_NEXT_SSA_BLOCK_LOOP,
    TYPE_NEXT_SSA_BLOCK_BRANCH,
    TYPE_NEXT_SSA_BLOCK_NONE, //OP_RETURN returns won't point to anything
} type_next_ssa_block_t;

//A block represents a list of instructions that run sequentially, there are no branches inside it
//If there is a branch instruction, this block will point to a different block which will contain the branched code
struct ssa_block {
    struct ssa_control_node * first; //Inclusive
    struct ssa_control_node * last; //Inclusive

    //Set of local variable numbers that this block will use before assigning
    //Example: i = i + 1; print a; a = 12; b = 1; print b. use_before_assigment: {i, a}
    //This facilitates the insertion of phi functions in loop bodies. So before using a variable, we
    //know what variables to copy
    struct u8_set use_before_assigment;

    //Indicates if the current block belongs to the body of a loop
    bool loop_body;

    //Set of pointers to ssa_block that points to this ssa_block
    struct u64_set predecesors;

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
void remove_control_node_ssa_block(struct ssa_block *, struct ssa_control_node *);
bool is_emtpy_ssa_block(struct ssa_block *);
//Replaces references to old_block of the predecessors of old_block to point to new_block
//Example: A -> B -> C. replace_block_ssa_block(old_block = B, new_block = C), the result: A -> C
void replace_block_ssa_block(struct ssa_block * old_block, struct ssa_block * new_block);
//Removes a true/false branch/block and the subsequent children of the branch/block to remove (subgraph)
struct branch_removed {
    struct u64_set ssa_name_definitions_removed;
    struct u64_set blocks_removed;
};
struct branch_removed remove_branch_ssa_ir(struct ssa_block *, bool true_branch, struct lox_allocator *);

type_next_ssa_block_t get_type_next_ssa_block(struct ssa_control_node *);
