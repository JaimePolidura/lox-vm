#pragma once

#include "runtime/jit/advanced/ssa_block.h"
#include "compiler/compiler.h"
#include "test.h"

extern struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct bytecode_list * start_function_bytecode
);

extern struct ssa_block * create_ssa_ir_blocks(
        struct ssa_control_node * start
);

#define ASSERT_PRINTS_NUMBER(node, value) { \
    struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) (node); \
    ASSERT_TRUE(print_node->control.type == SSA_CONTROL_NODE_TYPE_PRINT); \
    ASSERT_TRUE(print_node->data->type == SSA_DATA_NODE_TYPE_CONSTANT); \
    ASSERT_TRUE(((struct ssa_data_constant_node *) print_node->data)->as.i64 == value); \
}; \

#define NEXT_NODE(type, node) (type *) ((node)->control.next.next)
#define FALSE_BRANCH_NODE(type, node) (type *) ((node)->control.next.branch.false_branch)
#define TRUE_BRANCH_NODE(type, node) (type *) ((node)->control.next.branch.true_branch)

//Should produce:
//First block: [c = 1, ¿a > 0?]
//  -> True: [b > 0]
//      -> True: [b = 3; a = 2] -> FINAL BLOCK
//      -> False: [b = 3] -> FINAL BLOCK
//  -> False: [c = 1, var i = 0] -> [¿i < 10?]
//      -> True: [print 1, i = i + 1] -> [¿i < 10?]
//      -> False: FINAL BLOCK
//FINAL BLOCK: [print a; print b];
TEST(ssa_ir_block_creation){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b, c) {"
            "   c = 1;"
            "   if(a > 0) {"
            "       if(b > 0) {"
            "           b = 3;"
            "           a = 2;"
            "       } else {"
            "           b = 3;"
            "       }"
            "   } else {"
            "       c = 1;"
            "       for(var i = 0; i < 10; i = i + 1){"
            "           print 1;"
            "       }"
            "   }"
            "   print a;"
            "   print b;"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    int n_instructions = function_ssa->chunk->in_use;
    init_function_profile_data(&function_ssa->state_as.profiling.profile_data, n_instructions, function_ssa->n_locals);

    struct ssa_control_node * start_ssa_ir = create_ssa_ir_no_phis(
            package, function_ssa, create_bytecode_list(function_ssa->chunk)
    );
    struct ssa_block * ssa_block = create_ssa_ir_blocks(start_ssa_ir);
    ssa_block = ssa_block->next.next;

    // [c = 1, ¿a < 0?]
    ASSERT_EQ(ssa_block->first->type, SSA_CONTROL_NODE_TYPE_DATA); //Asignment
    ASSERT_EQ(ssa_block->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //a < 0
    ASSERT_EQ(ssa_block->outputs.in_use, 1);
    ASSERT_EQ(ssa_block->inputs.in_use, 1);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?]
    struct ssa_block * ssa_block_true = ssa_block->next.branch.true_branch;
    ASSERT_EQ(ssa_block_true->first->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //Asignment
    ASSERT_EQ(ssa_block_true->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //a < 0
    ASSERT_EQ(ssa_block_true->last, ssa_block_true->first); //a < 0
    ASSERT_EQ(ssa_block_true->outputs.in_use, 0);
    ASSERT_EQ(ssa_block_true->inputs.in_use, 1);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(true)-> [b = 3; a = 2]
    struct ssa_block * ssa_block_true_true = ssa_block_true->next.branch.true_branch;
    ASSERT_EQ(ssa_block_true_true->first->type, SSA_CONTROL_NODE_TYPE_DATA); //b = 3
    ASSERT_EQ(ssa_block_true_true->last->type, SSA_CONTROL_NODE_TYPE_DATA); //a = 2
    ASSERT_EQ(ssa_block_true_true->outputs.in_use, 2);
    ASSERT_EQ(ssa_block_true_true->inputs.in_use, 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(true)-> [b = 3; a = 2] -> FINAL BLOCK
    struct ssa_block * final_block = ssa_block_true_true->next.next;
    ASSERT_EQ(final_block->first->type, SSA_CONTROL_NODE_TYPE_PRINT); //print a;
    ASSERT_EQ(final_block->last->type, SSA_CONTROL_NODE_TYPE_RETURN); //final return OP_NIL OP_RETURN
    ASSERT_EQ(final_block->outputs.in_use, 0);
    ASSERT_EQ(final_block->inputs.in_use, 2);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(false)-> [b = 3]
    struct ssa_block * ssa_block_true_false = ssa_block_true->next.branch.false_branch;
    ASSERT_EQ(ssa_block_true_false->first->type, SSA_CONTROL_NODE_TYPE_DATA); //b = 3
    ASSERT_EQ(ssa_block_true_false->last->type, SSA_CONTROL_NODE_TYPE_DATA); //b = 3
    ASSERT_EQ(ssa_block_true_false->last, ssa_block_true_false->first); //a < 0
    ASSERT_EQ(ssa_block_true_false->outputs.in_use, 1);
    ASSERT_EQ(ssa_block_true_false->inputs.in_use, 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(false)-> [b = 3] -> FINAL BLOCK
    ASSERT_EQ(ssa_block_true_false->next.next, final_block);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0]
    struct ssa_block * ssa_block_false = ssa_block->next.branch.false_branch;
    ASSERT_EQ(ssa_block_false->first->type, SSA_CONTROL_NODE_TYPE_DATA); //c = 1
    ASSERT_EQ(ssa_block_false->last->type, SSA_CONTROL_NODE_TYPE_DATA); //i = 0
    ASSERT_EQ(ssa_block_false->outputs.in_use, 2);
    ASSERT_EQ(ssa_block_false->inputs.in_use, 0);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?]
    struct ssa_block * ssa_block_false_for_condition = ssa_block_false->next.next;
    ASSERT_EQ(ssa_block_false_for_condition->first->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //i < 10
    ASSERT_EQ(ssa_block_false_for_condition->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);
    ASSERT_EQ(ssa_block_false_for_condition->last, ssa_block_false_for_condition->first);
    ASSERT_EQ(ssa_block_false_for_condition->outputs.in_use, 0);
    ASSERT_EQ(ssa_block_false_for_condition->inputs.in_use, 1);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?] -(true)-> [print 1, i = i + 1, loop]
    struct ssa_block * ssa_block_false_for_condition_true = ssa_block_false_for_condition->next.branch.true_branch;
    ASSERT_EQ(ssa_block_false_for_condition_true->first->type, SSA_CONTROL_NODE_TYPE_PRINT); //print 1
    ASSERT_EQ(ssa_block_false_for_condition_true->first->next.next->type, SSA_CONTROL_NODE_TYPE_DATA); //i = i + 1
    ASSERT_EQ(ssa_block_false_for_condition_true->last->type, SSA_CONTROL_NODE_TYPE_LOOP_JUMP);
    ASSERT_EQ(ssa_block_false_for_condition_true->outputs.in_use, 1);
    ASSERT_EQ(ssa_block_false_for_condition_true->inputs.in_use, 1);
    ASSERT_EQ(ssa_block_false_for_condition_true->next.loop, ssa_block_false_for_condition);


    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?] -(false)-> FINAL BLOCK
    struct ssa_block * ssa_block_false_for_condition_false = ssa_block_false_for_condition->next.branch.false_branch;
    ASSERT_EQ(ssa_block_false_for_condition_false, final_block);
}

//Should produce:
// start -> a > 0 Conditional jump
// (true branch) -> b > 0 Conditional jump
//      (true branch) -> print 1 -> print 5
//      (false branch) -> print 2 -> print 5
// -> false branch -> i = 0 -> i < 10 Conditional branch
//      (true branch) -> print 3 -> i = i + 1 -> loop -> i = 0
//      (false branch) print 4 -> print 5
TEST(ssa_ir_no_phis){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b, c) {"
            "   if(a > 0) {"
            "       if(b > 0) {"
            "           print 1;"
            "       } else {"
            "           print 2;"
            "       }"
            "   } else {"
            "       for(var i = 0; i < 10; i = i + 1){"
            "           print 3;"
            "       }"
            "       print 4;"
            "   }"
            "   print 5;"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    int n_instructions = function_ssa->chunk->in_use;
    init_function_profile_data(&function_ssa->state_as.profiling.profile_data, n_instructions, function_ssa->n_locals);

    struct ssa_control_node * start_ssa_ir = create_ssa_ir_no_phis(
            package, function_ssa, create_bytecode_list(function_ssa->chunk)
    );

    //a > 0
    struct ssa_control_conditional_jump_node * a_condition = (struct ssa_control_conditional_jump_node *) start_ssa_ir->next.next;
    ASSERT_TRUE(a_condition->control.type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);

    //a > 0 True -> b > 0
    struct ssa_control_conditional_jump_node * a_condition_false_b_condition = TRUE_BRANCH_NODE(struct ssa_control_conditional_jump_node, a_condition);
    ASSERT_TRUE(a_condition_false_b_condition->control.type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);

    struct ssa_control_node * a_condition_false_b_condition_false = FALSE_BRANCH_NODE(struct ssa_control_node, a_condition_false_b_condition);
    struct ssa_control_node * a_condition_false_b_condition_true = TRUE_BRANCH_NODE(struct ssa_control_node, a_condition_false_b_condition);

    //a > 0 True -> b > 0 -> True
    ASSERT_PRINTS_NUMBER(a_condition_false_b_condition_true, 1);
    //a > 0 True -> b > 0 -> False
    ASSERT_PRINTS_NUMBER(a_condition_false_b_condition_false, 2);
    //Final print
    ASSERT_PRINTS_NUMBER(a_condition_false_b_condition_false->next.next, 5);
    ASSERT_PRINTS_NUMBER(a_condition_false_b_condition_true->next.next, 5);

    //a > 0 False -> var i = 0
    struct ssa_control_data_node * a_condition_false_loop_init = FALSE_BRANCH_NODE(struct ssa_control_data_node, a_condition);
    ASSERT_TRUE(a_condition_false_loop_init->data->type == SSA_DATA_NODE_TYPE_SET_LOCAL);
    struct ssa_data_set_local_node * set_i_0 = (struct ssa_data_set_local_node *) a_condition_false_loop_init->data;
    ASSERT_TRUE(set_i_0->data.type == SSA_DATA_NODE_TYPE_SET_LOCAL);
    ASSERT_TRUE(((struct ssa_data_constant_node *) set_i_0->new_local_value)->as.i64 == 0);

    //a > 0 False -> var i = 0; i < 10
    struct ssa_control_conditional_jump_node * a_condition_false_loop_condition = NEXT_NODE(struct ssa_control_conditional_jump_node, a_condition_false_loop_init);
    ASSERT_EQ(a_condition_false_loop_condition->control.type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);
    ASSERT_EQ(a_condition_false_loop_condition->condition->type, SSA_DATA_NODE_TYPE_COMPARATION);

    //a > 0 False -> var i = 0; i < 10 True
    struct ssa_control_node * a_condition_false_loop_body = TRUE_BRANCH_NODE(struct ssa_control_node, a_condition_false_loop_condition);
    ASSERT_PRINTS_NUMBER(a_condition_false_loop_body, 3);
    //a > 0 False -> var i = 0; i < 10 True: print 3; i = i + 1
    struct ssa_control_data_node * increment_i = (struct ssa_control_data_node *) a_condition_false_loop_body->next.next;
    ASSERT_EQ(increment_i->data->type, SSA_DATA_NODE_TYPE_SET_LOCAL);
    //a > 0 False -> var i = 0; i < 10 True: print 3; i = i + 1; go back to i < 10
    struct ssa_control_loop_jump_node * loop_node = (struct ssa_control_loop_jump_node *) increment_i->control.next.next;
    ASSERT_EQ(loop_node->control.type, SSA_CONTROL_NODE_TYPE_LOOP_JUMP);
    ASSERT_EQ((uint64_t) loop_node->control.next.next, (uint64_t) a_condition_false_loop_condition);

    struct ssa_control_node * a_condition_false_after_loop = FALSE_BRANCH_NODE(struct ssa_control_node, a_condition_false_loop_condition);
    ASSERT_PRINTS_NUMBER(a_condition_false_after_loop, 4);
    //Final print
    ASSERT_PRINTS_NUMBER(a_condition_false_after_loop->next.next, 5);
}
