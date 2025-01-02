#pragma once

#include "runtime/jit/advanced/visualization/graphviz_visualizer.h"
#include "runtime/jit/advanced/creation/ssa_no_phis_creator.h"
#include "runtime/jit/advanced/creation/ssa_phi_inserter.h"
#include "runtime/jit/advanced/creation/ssa_creator.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u8_set.h"
#include "compiler/compiler.h"
#include "../test.h"

static bool node_uses_version(struct ssa_data_node * start_node, int n_expected_version);
static bool node_uses_phi_versions(struct ssa_data_node * start_node, int n_expected_versions, ...);
static bool node_defines_ssa_name(struct ssa_control_node *, int version);

TEST(ssa_creation_ta){
    struct compilation_result compilation = compile_standalone(
            "fun function(a, b) {"
            "   var mensaje = \"a\";"
            "   var numero = 2;"
            "   if(a > numero) {"
            "       numero = a;"
            "   }"
            "   mensaje = mensaje + numero;"
            "   return numero;"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            UNBOXING_INSERTION_PHASE_SSA_GRAPHVIZ,
            DEFAULT_GRAPHVIZ_OPT | DISPLAY_TYPE_INFO_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_cp) {
    struct compilation_result compilation = compile_standalone(
            "fun function(a, b, c) {"
            "   for (var i = 0; i < 10; i = i + 1) {"
            "       print i;"
            "   }"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            COPY_PROPAGATION_PHASE_SSA_GRAPHVIZ,
            DEFAULT_GRAPHVIZ_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_pg) {
    struct compilation_result compilation = compile_standalone(
            "fun function(arr) {"
            "   for (var i = 0; i < 10; i = i + 1) {"
            "   }"
            "}"
    );
    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            ALL_PHASE_SSA_GRAPHVIZ,
            NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_licm) {
    struct compilation_result compilation = compile_standalone(
            "fun function(a, b, c) {"
            "   for (var i = 0; i < 10; i = i + 1) {"
            "      if(i == 0){"
            "          var d = a + b;"
            "          var e = e + i;"
            "      }else{"
            "          for(var j = 0; j < 10; j = j + 1){"
            "              print i + 1;"
            "              c = 2;"
            "              var d = i * 2;"
            "              print d;"
            "           }"
            "       }"
            "       print c + 3;"
            "   }"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            NO_PHIS_PHASE_SSA_GRAPHVIZ,
            NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_sr){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b) {"
            "   if(a % 2 == 0) {"
            "   }"
            "   return (a % 10) + (b % 8);"
            "}"
    );
    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            STRENGTH_REDUCTION_PHASE_SSA_GRAPHVIZ,
            NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_cse){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b) {"
            "   var c = a + b;"
            "   var d = 2 * (b + a);"
            "   if(a > 0) {"
            "       print a - b;"
            "       if (a > 0) {"
            "           print a * b;"
            "           return 2 / (a - b);"
            "       } else {"
            "           return a * b;"
            "       }"
            "   }"
            "}"
    );
    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            COMMON_SUBEXPRESSION_ELIMINATION_PHASE_SSA_GRAPHVIZ,
            NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_nested_loop){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b) {"
            "   for(var i = 0; i < 10; i = i + 1) {"
            "       for(var j = i; j < i; j = j + 1) {"
            "       }"
            "   }"
            "}"
    );
    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    init_function_profile_data(function_ssa);

    //Observe the generated graph IR
    generate_ssa_graphviz_graph(
            package,
            function_ssa,
            PHIS_INSERTED_PHASE_SSA_GRAPHVIZ,
            NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT,
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE,
            "C:\\Users\\jaime\\OneDrive\\Escritorio\\ir.txt"
    );
}

TEST(ssa_creation_scp) {
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b) {"
            "   a = 1;"
            "   if(a > 0) {"
            "      if(b > 0) {"
            "          a = 12 + a + 2;"
            "      } else {"
            "          for(var i = 0; i < 10; i = i + 1) {"
            "              b = 12;"
            "          }"
            "      }"
            "   } else {"
            "      b = 10;"
            "   }"
            "   print b;"
            "   print a;"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    init_function_profile_data(function_ssa);
    struct ssa_ir ssa_ir = create_ssa_ir(package, function_ssa, create_bytecode_list(function_ssa->chunk, NATIVE_LOX_ALLOCATOR()),
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE);
    perform_sparse_constant_propagation(&ssa_ir);

    //Observe the generated graph IR

    ASSERT_EQ(ssa_ir.first_block->type_next_ssa_block, TYPE_NEXT_SSA_BLOCK_SEQ);
    struct ssa_block * final_block = ssa_ir.first_block->next_as.branch.true_branch->next_as.next->next_as.next;
    struct ssa_control_define_ssa_name_node * print_b_extract = (struct ssa_control_define_ssa_name_node *) final_block->first;
    ASSERT_TRUE(node_uses_phi_versions(print_b_extract->value, 2, 0, 1));
}

//Expect
//[a1 = 1; ¿a1 > 0?]
//  -> True: [¿b0 > 0?]
//      -> True: [b1 = 3; a2 = 3] -> FINAL BLOCK
//      -> False: [b2 = 3] -> FINAL BLOCK
//  -> False: [a3 = 3; i1 = 1] -> [i4 = phi(i1, 13) ¿i4 < 10?]
//      -> True: [b3 = 12; i2 = i4; i3 = i2 + 1;]
//      -> False: FINAL BLOCK
//FINAL BLOCK: [a4 = phi(a1, a2, a3); print a4; b4 = phi(b0, b1, b2, b3); print b4]
TEST(ssa_creation_phis_inserter_and_optimizer){
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
    init_function_profile_data(function_ssa);

    struct ssa_ir ssa_ir = create_ssa_ir(package, function_ssa, create_bytecode_list(function_ssa->chunk, NATIVE_LOX_ALLOCATOR()),
            SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE);
    struct ssa_block * start_ssa_block = ssa_ir.first_block;

    ASSERT_EQ(size_u64_set(start_ssa_block->predecesors), 0);
    ASSERT_TRUE(node_defines_ssa_name(start_ssa_block->first, 1)); //a1 = 1;
    //a1 > 0
    struct ssa_control_conditional_jump_node * a_condition = (struct ssa_control_conditional_jump_node *) start_ssa_block->first->next;
    ASSERT_TRUE(node_uses_phi_versions(a_condition->condition, 1, 1));

    struct ssa_block * a_condition_true = start_ssa_block->next_as.branch.true_branch;
    ASSERT_EQ(size_u64_set(a_condition_true->predecesors), 1);
    //b0 > 0
    struct ssa_control_conditional_jump_node * a_condition_true_b_condition = (struct ssa_control_conditional_jump_node *) a_condition_true->first;
    ASSERT_TRUE(node_uses_phi_versions(a_condition_true_b_condition->condition, 1, 0));

    struct ssa_block * a_condition_true_b_condition_true = a_condition_true->next_as.branch.true_branch;
    ASSERT_EQ(size_u64_set(a_condition_true_b_condition_true->predecesors), 1);
    ASSERT_TRUE(node_defines_ssa_name(a_condition_true_b_condition_true->first, 1)); //b1 = 3;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_true_b_condition_true->first->next, 2)); //a2 = 2;

    struct ssa_block * a_condition_true_b_condition_false = a_condition_true->next_as.branch.false_branch;
    struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) a_condition_true_b_condition_false->first;
    ASSERT_EQ(size_u64_set(a_condition_true_b_condition_false->predecesors), 1);
    ASSERT_TRUE(node_defines_ssa_name(a_condition_true_b_condition_false->first, 2));  //b2 = 3;

    struct ssa_block * a_condition_false = start_ssa_block->next_as.branch.false_branch;
    ASSERT_EQ(size_u64_set(a_condition_false->predecesors), 1);
    ASSERT_TRUE(node_defines_ssa_name(a_condition_false->first, 3)); //a3 = 1;
    ASSERT_TRUE(node_defines_ssa_name(a_condition_false->first->next, 1)); //i1 = 1;

    //Loop condition
    struct ssa_block * for_loop_condition_block = a_condition_false->next_as.next;
    ASSERT_TRUE(node_defines_ssa_name(for_loop_condition_block->first, 4)); //i4 = phi(i1, i3)
    struct ssa_control_define_ssa_name_node * define_i = (struct ssa_control_define_ssa_name_node *) for_loop_condition_block->first;
    ASSERT_TRUE(node_uses_phi_versions(define_i->value, 2, 1, 3)); //i4 = phi(i1, i3)
    struct ssa_control_conditional_jump_node * for_loop_condition = (struct ssa_control_conditional_jump_node *) for_loop_condition_block->first->next;
    ASSERT_TRUE(node_uses_version(for_loop_condition->condition, 4)); //i4 < 10
    //Loop body
    struct ssa_block * for_loop_body_block = for_loop_condition_block->next_as.branch.true_branch;
    ASSERT_TRUE(node_defines_ssa_name(for_loop_body_block->first, 3)); //b3 = 12;
    //Loop increment
    struct ssa_control_set_local_node * extract_i_loop = (struct ssa_control_set_local_node *) for_loop_body_block->first->next;
    ASSERT_TRUE(node_defines_ssa_name(for_loop_body_block->first->next, 2)); //i2 = i4
    ASSERT_TRUE(node_uses_phi_versions(extract_i_loop->new_local_value, 1, 4)); //i2 = i4
    struct ssa_control_set_local_node * increment_i_loop = (struct ssa_control_set_local_node *) extract_i_loop->control.next;
    ASSERT_TRUE(node_defines_ssa_name(&increment_i_loop->control, 3)); //i3 = i2 + 1;
    ASSERT_TRUE(node_uses_phi_versions(increment_i_loop->new_local_value, 1, 2)); //i3 = i2 + 1;

    //Final block
    struct ssa_block * final_block = a_condition_true_b_condition_true->next_as.next;
    struct ssa_control_define_ssa_name_node * final_block_print_a = (struct ssa_control_define_ssa_name_node *) final_block->first;
    ASSERT_EQ(size_u64_set(final_block->predecesors), 3);
    ASSERT_TRUE(node_defines_ssa_name(&final_block_print_a->control, 4)); //a4 = phi(a1, a2, a3);
    ASSERT_TRUE(node_uses_phi_versions(final_block_print_a->value, 3, 1, 2, 3));
    struct ssa_control_print_node * print_a_node = (struct ssa_control_print_node *) final_block_print_a->control.next;
    ASSERT_TRUE(node_uses_version(print_a_node->data, 4)); //print a4;

    struct ssa_control_define_ssa_name_node * final_block_print_b = (struct ssa_control_define_ssa_name_node *) final_block->first->next->next;
    ASSERT_TRUE(node_defines_ssa_name(&final_block_print_b->control, 4)); //b4 = phi(b0, b1, b2, b3);
    ASSERT_TRUE(node_uses_phi_versions(final_block_print_b->value, 4, 0, 1, 2, 3));
    struct ssa_control_print_node * print_b_node = (struct ssa_control_print_node *) final_block_print_b->control.next;
    ASSERT_TRUE(node_uses_version(print_b_node->data, 4)); //print b4;
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
TEST(ssa_creation_no_phis) {
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
    init_function_profile_data(function_ssa);
    struct arena arena;
    init_arena(&arena);
    struct arena_lox_allocator arena_lox_allocator = to_lox_allocator_arena(arena);

    struct ssa_block * ssa_block = create_ssa_ir_no_phis(
            package, function_ssa, create_bytecode_list(function_ssa->chunk, NATIVE_LOX_ALLOCATOR()),
            &arena_lox_allocator, SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE
    );

    // [c = 1, ¿a < 0?]
    ASSERT_EQ(ssa_block->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //Asignment
    ASSERT_EQ(ssa_block->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //a < 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?]
    struct ssa_block * ssa_block_true = ssa_block->next_as.branch.true_branch;
    ASSERT_EQ(ssa_block_true->first->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //Asignment
    ASSERT_EQ(ssa_block_true->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //a < 0
    ASSERT_EQ(ssa_block_true->last, ssa_block_true->first); //a < 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(true)-> [b = 3; a = 2]
    struct ssa_block * ssa_block_true_true = ssa_block_true->next_as.branch.true_branch;
    ASSERT_EQ(ssa_block_true_true->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //b = 3
    ASSERT_EQ(ssa_block_true_true->last->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //a = 2
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(true)-> [b = 3; a = 2] -> FINAL BLOCK
    struct ssa_block * final_block = ssa_block_true_true->next_as.next;
    ASSERT_EQ(final_block->first->type, SSA_CONTROL_NODE_TYPE_PRINT); //print a;
    ASSERT_EQ(final_block->last->type, SSA_CONTROL_NODE_TYPE_RETURN); //final return OP_NIL OP_RETURN
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(false)-> [b = 3]
    struct ssa_block * ssa_block_true_false = ssa_block_true->next_as.branch.false_branch;
    ASSERT_EQ(ssa_block_true_false->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //b = 3
    ASSERT_EQ(ssa_block_true_false->last->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //b = 3
    ASSERT_EQ(ssa_block_true_false->last, ssa_block_true_false->first); //a < 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(true)-> [¿b > 0?] -(false)-> [b = 3] -> FINAL BLOCK
    ASSERT_EQ(ssa_block_true_false->next_as.next, final_block);
    ASSERT_EQ(final_block->next_as.next, NULL);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0]
    struct ssa_block * ssa_block_false = ssa_block->next_as.branch.false_branch;
    ASSERT_EQ(ssa_block_false->first->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //c = 1
    ASSERT_EQ(ssa_block_false->last->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //i = 0
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?]
    struct ssa_block * ssa_block_false_for_condition = ssa_block_false->next_as.next;
    ASSERT_EQ(ssa_block_false_for_condition->first->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP); //i < 10
    ASSERT_EQ(ssa_block_false_for_condition->last->type, SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP);
    ASSERT_EQ(ssa_block_false_for_condition->last, ssa_block_false_for_condition->first);
    ASSERT_EQ(size_u8_set(ssa_block->use_before_assigment), 0);

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?] -(true)-> [print 1, i = i + 1, loop]
    struct ssa_block * ssa_block_false_for_condition_true = ssa_block_false_for_condition->next_as.branch.true_branch;
    ASSERT_EQ(ssa_block_false_for_condition_true->first->type, SSA_CONTROL_NODE_TYPE_PRINT); //print 1
    ASSERT_EQ(ssa_block_false_for_condition_true->first->next->type, SSA_CONTORL_NODE_TYPE_SET_LOCAL); //i = i + 1
    ASSERT_EQ(ssa_block_false_for_condition_true->last->type, SSA_CONTROL_NODE_TYPE_LOOP_JUMP);
    ASSERT_EQ(ssa_block_false_for_condition_true->next_as.loop, ssa_block_false_for_condition);
    ASSERT_EQ(size_u8_set(ssa_block_false_for_condition_true->use_before_assigment), 1);
    ASSERT_TRUE(contains_u8_set(&ssa_block_false_for_condition_true->use_before_assigment, 4));

    //[c = 1, ¿a < 0?] -(false)-> [c = 1, i = 0] -> [¿i < 10?] -(false)-> FINAL BLOCK
    struct ssa_block * ssa_block_false_for_condition_false = ssa_block_false_for_condition->next_as.branch.false_branch;
    ASSERT_EQ(ssa_block_false_for_condition_false, final_block);

    free_arena(&arena_lox_allocator.arena);
}

static bool node_defines_ssa_name(struct ssa_control_node * node, int version) {
    struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) node;
    return define_ssa_name->ssa_name.value.version == version;
}

static void int_array_to_set(struct u64_set *, int n_array_elements, int array[n_array_elements]);

static bool node_uses_version(struct ssa_data_node * start_node, int expected_version) {
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, start_node);

    while(!is_empty_stack_list(&pending)) {
        struct ssa_data_node *current = pop_stack_list(&pending);
        switch (current->type) {
            case SSA_DATA_NODE_TYPE_PHI: {
                struct ssa_data_phi_node *phi_node = (struct ssa_data_phi_node *) current;
                struct u64_set_iterator definitions_iterator;
                init_u64_set_iterator(&definitions_iterator, phi_node->ssa_versions);

                while (has_next_u64_set_iterator(definitions_iterator)) {
                    uint64_t actual_version =  (uint64_t) next_u64_set_iterator(&definitions_iterator);

                    if (actual_version != expected_version) {
                        free_stack_list(&pending);
                        return false;
                    }
                }

                return true;
            }
            case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
                struct ssa_data_get_ssa_name_node *get_ssa_name = (struct ssa_data_get_ssa_name_node *) current;
                free_stack_list(&pending);
                return get_ssa_name->ssa_name.value.version == expected_version;
            }
            case SSA_DATA_NODE_TYPE_BINARY: {
                struct ssa_data_binary_node *binary_node = (struct ssa_data_binary_node *) current;
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

//Expect simple expressions, that will contain as much as 1 variable reference
static bool node_uses_phi_versions(struct ssa_data_node * start_node, int n_expected_versions, ...) {
    int expected_versions[n_expected_versions];
    VARARGS_TO_ARRAY(int, expected_versions, n_expected_versions);
    struct u64_set expected_versions_set;
    init_u64_set(&expected_versions_set, NATIVE_LOX_ALLOCATOR());
    int_array_to_set(&expected_versions_set, n_expected_versions, expected_versions);

    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, start_node);

    while(!is_empty_stack_list(&pending)){
        struct ssa_data_node * current = pop_stack_list(&pending);
        switch(current->type) {
            case SSA_DATA_NODE_TYPE_PHI: {
                struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current;
                struct u64_set_iterator definitions_iterator;
                init_u64_set_iterator(&definitions_iterator, phi_node->ssa_versions);

                while(has_next_u64_set_iterator(definitions_iterator)) {
                    uint64_t actual_version = (uint64_t) next_u64_set_iterator(&definitions_iterator);

                    if(!contains_u64_set(&expected_versions_set, actual_version)){
                        free_stack_list(&pending);
                        return false;
                    }
                }

                return true;
            }
            case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
                struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) current;
                free_stack_list(&pending);
                return contains_u64_set(&expected_versions_set, get_ssa_name->ssa_name.value.version);
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