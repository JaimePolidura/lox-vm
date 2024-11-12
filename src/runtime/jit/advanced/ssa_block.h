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

    //Set of local variable numbers that this block will use from outside
    //Example: print b; i = i + 1; Inputs: {b, i}
    //Use after assigments won't be included in the inputs set. Exmaple: a = 1; print a; Inputs: {}
    //It can also be defined as (i nº of instruction in the block):
    //Inputs = U (i start = 0) (Inputs(i) - Outputs(i - 1))
    struct u8_set inputs;
    //Set of local variable numbers that this block will define/assign
    //Example: print b; i = i + 1; Outputs: {i}
    //It can also be defined as (i nº of instruction in the block):
    //Outputs = U (i start = 0) Outputs(i)
    struct u8_set outputs;

    type_next_ssa_block_t type_next_ssa_block;

    union {
        struct ssa_block * next;
        struct ssa_block * loop;
        struct {
            struct ssa_block * true_branch;
            struct ssa_block * false_branch;
        } branch;
    } next;
};

struct ssa_block * alloc_ssa_block();
void init_ssa_block(struct ssa_block *);

type_next_ssa_block_t get_type_next_ssa_block(struct ssa_control_node *);
