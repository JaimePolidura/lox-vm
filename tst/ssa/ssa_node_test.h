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

static struct ssa_data_node * create_add_data_node1();
static struct ssa_data_node * create_add_data_node2();

//((a + b) + c) == (b + (a + c))
TEST(ssa_data_node_is_eq) {
    ASSERT_TRUE(is_eq_ssa_data_node(create_add_data_node1(), create_add_data_node2(), NATIVE_LOX_ALLOCATOR()));
}

//Hash((a + b) + c) == Hash(b + (a + c))
TEST(ssa_data_node_hash) {
    ASSERT_EQ(hash_ssa_data_node(create_add_data_node1()), hash_ssa_data_node(create_add_data_node2()));
}

//Returns: ((a + b) + c)
static struct ssa_data_node * create_add_data_node1() {
    struct ssa_data_get_ssa_name_node * get_a = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_a->ssa_name = CREATE_SSA_NAME(1, 1);
    struct ssa_data_get_ssa_name_node * get_b = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_b->ssa_name = CREATE_SSA_NAME(2, 1);
    struct ssa_data_get_ssa_name_node * get_c = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_c->ssa_name = CREATE_SSA_NAME(3, 1);

    //First tree: (a + b) + c
    struct ssa_data_binary_node * first_tree_add_a_b = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_BINARY, struct ssa_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    first_tree_add_a_b->right = &get_b->data;
    first_tree_add_a_b->left = &get_a->data;
    first_tree_add_a_b->operand = OP_ADD;
    struct ssa_data_binary_node * first_tree_head_node_add_c = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_BINARY, struct ssa_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    first_tree_head_node_add_c->right = &get_c->data;
    first_tree_head_node_add_c->left = &first_tree_add_a_b->data;
    first_tree_head_node_add_c->operand = OP_ADD;

    return &first_tree_head_node_add_c->data;
}

//Returns: b + (a + c)
static struct ssa_data_node * create_add_data_node2() {
    struct ssa_data_get_ssa_name_node * get_a = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_a->ssa_name = CREATE_SSA_NAME(1, 1);
    struct ssa_data_get_ssa_name_node * get_b = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_b->ssa_name = CREATE_SSA_NAME(2, 1);
    struct ssa_data_get_ssa_name_node * get_c = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_c->ssa_name = CREATE_SSA_NAME(3, 1);

    struct ssa_data_binary_node * second_tree_add_a_c = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_BINARY, struct ssa_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    second_tree_add_a_c->right = &get_c->data;
    second_tree_add_a_c->left = &get_a->data;
    second_tree_add_a_c->operand = OP_ADD;
    struct ssa_data_binary_node * second_tree_head_node_add_b = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_BINARY, struct ssa_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    second_tree_head_node_add_b->right = &second_tree_add_a_c->data;
    second_tree_head_node_add_b->left = &get_b->data;
    second_tree_head_node_add_b->operand = OP_ADD;

    return &second_tree_head_node_add_b->data;
}