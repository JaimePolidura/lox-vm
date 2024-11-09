#include "shared/utils/collections/u8_arraylist.h"

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

    //Local variable numbers that this block will need
    struct u8_arraylist inputs;
    //Local variable numbers that this block "outputs" (assigns or defines)
    struct u8_arraylist outputs;

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