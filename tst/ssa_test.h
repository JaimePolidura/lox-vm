#pragma once

#include "runtime/jit/advanced/ssa_creator.h"
#include "compiler/compiler.h"
#include "test.h"

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
// start -> a > 0 Conditional jump
// (true branch) -> b > 0 Conditional jump
//      (true branch) -> print 1 -> print 5
//      (false branch) -> print 2 -> print 5
// -> false branch -> i = 0 -> i < 10 Conditional branch
//      (true branch) -> print 3 -> i = i + 1 -> loop -> i = 0
//      (false branch) print 4 -> print 5
TEST(simple_ssa_ir_test){
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

    struct ssa_control_node * start_ssa_ir = create_ssa_ir(
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
