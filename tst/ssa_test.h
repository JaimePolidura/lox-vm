#pragma once

#include "shared/utils/collections/u8_set.h"
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

extern void insert_ssa_ir_phis(
        struct ssa_block * start_block
);

static bool node_uses_phi_versions(struct ssa_data_node * start_node, int n_expected_versions, ...);
static bool node_defines_ssa_name(struct ssa_control_node *, int version);

#define ASSERT_PRINTS_NUMBER(node, value) { \
    struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) (node); \
    ASSERT_TRUE(print_node->control.type == SSA_CONTROL_NODE_TYPE_PRINT); \
    ASSERT_TRUE(print_node->data->type == SSA_DATA_NODE_TYPE_CONSTANT); \
    ASSERT_TRUE(((struct ssa_data_constant_node *) print_node->data)->value_as.i64 == value); \
}; \

#define NEXT_NODE(type, node) (type *) ((node)->control.next.next)
#define FALSE_BRANCH_NODE(type, node) (type *) ((node)->control.next.branch.false_branch)
#define TRUE_BRANCH_NODE(type, node) (type *) ((node)->control.next.branch.true_branch)

//Expect
//First block: [a1 = 1; a1 > 0]
//  -> True: [¿b0 > 0?]
//      -> True: [b1 = 3; a2 = 3] -> FINAL BLOCK
//      -> False: [b2 = 3] -> FINAL BLOCK
//  -> False: [a3 = 3; i0 = 1] -> [¿phi(i0, i2) < 10?]
//      -> True: [b3 = 12; i1 = phi(i0, i2); i2 = i1 + 1;]
//      -> False: FINAL BLOCK
//FINAL BLOCK: [print phi(a1, a2, a3); print phi(b0, b1, b2, b3)]
TEST(ssa_phis_inserter){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b) {"
            "   a = 1;"
            "   if(a > 0) {"
            "       if(b > 0) {"
            "           b = 3;"
            "           a = 2;"
            "       } else {"
            "           b = 3;"
            "       }"
            "   } else {"
            "       a = 1;"
            "       for(var i = 0; i < 10; i = i + 1){"
            "           b = 12;"
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
    struct ssa_control_node * start_ssa_ir = create_ssa_ir_no_phis(package, function_ssa, create_bytecode_list(function_ssa->chunk));
    struct ssa_block * start_ssa_block = create_ssa_ir_blocks(start_ssa_ir);
    insert_ssa_ir_phis(start_ssa_block);
    start_ssa_block = start_ssa_block->next.next;

    ASSERT_TRUE(node_defines_ssa_name(start_ssa_block->first, 1)); //a1 = 1;
    //a1 > 0
    struct ssa_control_conditional_jump_node * a_condition = (struct ssa_control_conditional_jump_node *) start_ssa_block->first->next.next;
    ASSERT_TRUE(node_uses_phi_versions(a_condition->condition, 1, 1));

    struct ssa_block * a_condition_true = start_ssa_block->next.branch.true_branch;
    //b0 > 0
    struct ssa_control_conditional_jump_node * a_condition_true_b_condition = (struct ssa_control_conditional_jump_node *) a_condition_true->first;
    ASSERT_TRUE(node_uses_phi_versions(a_condition_true_b_condition->condition, 1, 0));

    struct ssa_block * a_condition_true_b_condition_true = a_condition_true->next.branch.true_branch;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_true_b_condition_true->first, 1)); //b1 = 3;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_true_b_condition_true->first->next.next, 2)); //a2 = 2;

    struct ssa_block * a_condition_true_b_condition_false = a_condition_true->next.branch.false_branch;
    struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) a_condition_true_b_condition_false->first;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_true_b_condition_false->first, 2));  //b2 = 3;

    struct ssa_block * a_condition_false = start_ssa_block->next.branch.false_branch;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_false->first, 3)); //a3 = 1;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_false->first->next.next, 1)); //i1 = 1;

    struct ssa_block * for_loop_condition_block = a_condition_false->next.next;
    struct ssa_control_conditional_jump_node * for_loop_condition = (struct ssa_control_conditional_jump_node *) for_loop_condition_block->first;
    ASSERT_TRUE(node_uses_phi_versions(for_loop_condition->condition, 2, 1, 3)); //phi(i1, i3) < 10

    struct ssa_block * for_loop_body_block = for_loop_condition_block->next.branch.true_branch;
    ASSERT_TRUE(node_defines_ssa_name(for_loop_body_block->first, 3)); //b3 = 12;

    struct ssa_control_set_local_node * extract_i_loop = (struct ssa_control_set_local_node *) for_loop_body_block->first->next.next;
    ASSERT_TRUE(node_defines_ssa_name(for_loop_body_block->first->next.next, 2)); //i2 = phi(i1, i3) + 1;

    ASSERT_TRUE(node_uses_phi_versions(extract_i_loop->new_local_value, 2, 1, 3)); //i2 = phi(i1, i3) + 1
    struct ssa_control_set_local_node * increment_i_loop = (struct ssa_control_set_local_node *) extract_i_loop->control.next.next;
    ASSERT_TRUE(node_defines_ssa_name(&increment_i_loop->control, 3)); //i3 = i2 + 1;

    ASSERT_TRUE(node_uses_phi_versions(increment_i_loop->new_local_value, 1, 2)); //i3 = i2 + 1;

    struct ssa_block * final_block = a_condition_true_b_condition_true->next.next;
    struct ssa_control_print_node * final_block_print_a = (struct ssa_control_print_node *) final_block->first;
    ASSERT_TRUE(node_uses_phi_versions(final_block_print_a->data, 3, 1, 2, 3)); //print phi(a0, a2, a3);
    struct ssa_control_print_node * final_block_print_b = (struct ssa_control_print_node *) final_block_print_a->control.next.next;
    ASSERT_TRUE(node_uses_phi_versions(final_block_print_b->data, 4, 0, 1, 2, 3)); //print phi(b0, b1, b2, b3);
}

//Should produce:
//First block: [c = 1; ¿a > 0?]
//  -> True: [b > 0]
//      -> True: [b = 3; a = 2] -> FINAL BLOCK
//      -> False: [b = 3] -> FINAL BLOCK
//  -> False: [c = 1, var i = 0] -> [¿i < 10?]
//      -> True: [print 1; i = i + 1] -> [¿i < 10?]
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
    ASSERT_EQ(ssa_block->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //Asignment
    ASSERT_EQ(ssa_block->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //a < 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?]
    struct ssa_block * ssa_block_true = ssa_block->next.branch.true_branch;
    ASSERT_EQ(ssa_block_true->first->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //Asignment
    ASSERT_EQ(ssa_block_true->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //a < 0
    ASSERT_EQ(ssa_block_true->last, ssa_block_true->first); //a < 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(true)-> [b = 3; a = 2]
    struct ssa_block * ssa_block_true_true = ssa_block_true->next.branch.true_branch;
    ASSERT_EQ(ssa_block_true_true->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //b = 3
    ASSERT_EQ(ssa_block_true_true->last->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //a = 2
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(true)-> [b = 3; a = 2] -> FINAL BLOCK
    struct ssa_block * final_block = ssa_block_true_true->next.next;
    ASSERT_EQ(final_block->first->type, SSA_CONTROL_NODE_TYPE_PRINT); //print a;
    ASSERT_EQ(final_block->last->type, SSA_CONTROL_NODE_TYPE_RETURN); //final return OP_NIL OP_RETURN
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(false)-> [b = 3]
    struct ssa_block * ssa_block_true_false = ssa_block_true->next.branch.false_branch;
    ASSERT_EQ(ssa_block_true_false->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //b = 3
    ASSERT_EQ(ssa_block_true_false->last->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //b = 3
    ASSERT_EQ(ssa_block_true_false->last, ssa_block_true_false->first); //a < 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(false)-> [b = 3] -> FINAL BLOCK
    ASSERT_EQ(ssa_block_true_false->next.next, final_block);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0]
    struct ssa_block * ssa_block_false = ssa_block->next.branch.false_branch;
    ASSERT_EQ(ssa_block_false->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //c = 1
    ASSERT_EQ(ssa_block_false->last->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //i = 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?]
    struct ssa_block * ssa_block_false_for_condition = ssa_block_false->next.next;
    ASSERT_EQ(ssa_block_false_for_condition->first->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //i < 10
    ASSERT_EQ(ssa_block_false_for_condition->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);
    ASSERT_EQ(ssa_block_false_for_condition->last, ssa_block_false_for_condition->first);
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?] -(true)-> [print 1, i = i + 1, loop]
    struct ssa_block * ssa_block_false_for_condition_true = ssa_block_false_for_condition->next.branch.true_branch;
    ASSERT_EQ(ssa_block_false_for_condition_true->first->type, SSA_CONTROL_NODE_TYPE_PRINT); //print 1
    ASSERT_EQ(ssa_block_false_for_condition_true->first->next.next->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //i = i + 1
    ASSERT_EQ(ssa_block_false_for_condition_true->last->type, SSA_CONTROL_NODE_TYPE_LOOP_JUMP);
    ASSERT_EQ(ssa_block_false_for_condition_true->next.loop, ssa_block_false_for_condition);
    ASSERT_EQ(size_u8_set(ssa_block_false_for_condition_true->use_before_assigment), 1);
    ASSERT_TRUE(contains_u8_set(&ssa_block_false_for_condition_true->use_before_assigment, 4));

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
    struct ssa_control_set_local_node * a_condition_false_loop_init = FALSE_BRANCH_NODE(struct ssa_control_set_local_node, a_condition);
    ASSERT_TRUE(a_condition_false_loop_init->control.type == SSA_CONTORL_NODE_TYPE_SET_LOCAL);
    ASSERT_TRUE(((struct ssa_data_constant_node *) a_condition_false_loop_init->new_local_value)->value_as.i64 == 0);

    //a > 0 False -> var i = 0; i < 10
    struct ssa_control_conditional_jump_node * a_condition_false_loop_condition = NEXT_NODE(struct ssa_control_conditional_jump_node, a_condition_false_loop_init);
    ASSERT_EQ(a_condition_false_loop_condition->control.type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);
    ASSERT_EQ(a_condition_false_loop_condition->condition->type, SSA_DATA_NODE_TYPE_BINARY);

    //a > 0 False -> var i = 0; i < 10 True
    struct ssa_control_node * a_condition_false_loop_body = TRUE_BRANCH_NODE(struct ssa_control_node, a_condition_false_loop_condition);
    ASSERT_PRINTS_NUMBER(a_condition_false_loop_body, 3);
    //a > 0 False -> var i = 0; i < 10 True: print 3; i = i + 1
    struct ssa_control_set_local_node * increment_i = (struct ssa_control_set_local_node *) a_condition_false_loop_body->next.next;
    ASSERT_EQ(increment_i->control.type, SSA_CONTORL_NODE_TYPE_SET_LOCAL);
    //a > 0 False -> var i = 0; i < 10 True: print 3; i = i + 1; go back to i < 10
    struct ssa_control_loop_jump_node * loop_node = (struct ssa_control_loop_jump_node *) increment_i->control.next.next;
    ASSERT_EQ(loop_node->control.type, SSA_CONTROL_NODE_TYPE_LOOP_JUMP);
    ASSERT_EQ((uint64_t) loop_node->control.next.next, (uint64_t) a_condition_false_loop_condition);

    struct ssa_control_node * a_condition_false_after_loop = FALSE_BRANCH_NODE(struct ssa_control_node, a_condition_false_loop_condition);
    ASSERT_PRINTS_NUMBER(a_condition_false_after_loop, 4);
    //Final print
    ASSERT_PRINTS_NUMBER(a_condition_false_after_loop->next.next, 5);
}

static bool node_defines_ssa_name(struct ssa_control_node * node, int version) {
    struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) node;
    return define_ssa_name->ssa_name.value.version == version;
}

static void int_array_to_set(struct u64_set *, int n_array_elements, int array[n_array_elements]);

static bool node_uses_phi_versions(struct ssa_data_node * start_node, int n_expected_versions, ...) {
    int expected_versions[n_expected_versions];
    VARARGS_TO_ARRAY(int, expected_versions, n_expected_versions);
    struct u64_set expected_versions_set;
    init_u64_set(&expected_versions_set);
    int_array_to_set(&expected_versions_set, n_expected_versions, expected_versions);

    struct stack_list pending;
    init_stack_list(&pending);
    push_stack_list(&pending, start_node);

    while(!is_empty_stack_list(&pending)){
        struct ssa_data_node * current = pop_stack_list(&pending);
        switch(current->type) {
            case SSA_DATA_NODE_TYPE_PHI: {
                struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current;
                struct u64_set_iterator definitions_iterator;
                init_u64_set_iterator(&definitions_iterator, phi_node->ssa_definitions);

                while(has_next_u64_set_iterator(definitions_iterator)) {
                    struct ssa_control_define_ssa_name_node * define_ssa_node = (void *) next_u64_set_iterator(&definitions_iterator);
                    uint64_t actual_version = define_ssa_node->ssa_name.value.version;

                    if(!contains_u64_set(&expected_versions_set, actual_version)){
                        return false;
                    }
                }

                return true;
            }
            case SSA_DATA_NODE_TYPE_BINARY: {
                struct ssa_data_binary_node * binary_node = (struct ssa_data_binary_node *) current;
                push_stack_list(&pending, binary_node->right);
                push_stack_list(&pending, binary_node->left);
                break;
            }
            case SSA_DATA_NODE_TYPE_CONSTANT:
            case SSA_DATA_NODE_TYPE_CALL:
            case SSA_DATA_NODE_TYPE_GET_GLOBAL:
            case SSA_DATA_NODE_TYPE_UNARY:
            case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
            case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
            case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
            case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
                break;
        }
    }

    free_stack_list(&pending);

    return false;
}

static void int_array_to_set(struct u64_set * set, int n_array_elements, int array[n_array_elements]) {
    for (int i = 0; i < n_array_elements; ++i) {
        add_u64_set(set, (uint64_t) array[i]);
    }
}