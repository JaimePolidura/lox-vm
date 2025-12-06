#pragma once

#include "runtime/jit/advanced/visualization/lox_ir_graphviz_visualizer.h"
#include "runtime/jit/advanced/creation/no_phis_creator.h"
#include "runtime/jit/advanced/creation/phi_inserter.h"
#include "runtime/jit/advanced/creation/lox_ir_creator.h"
#include "runtime/jit/advanced/optimizations/sparse_constant_propagation.h"
#include "runtime/jit/advanced/lox_ir_block.h"

#include "shared/utils/collections/u8_set.h"
#include "compiler/compiler.h"
#include "../test.h"

static struct lox_ir_data_node * create_add_data_node1();
static struct lox_ir_data_node * create_add_data_node2();

//A -> [B, C] B -> [D, E] C -> [F] D -> [G] E -> [G] G -> [H] F -> [H]
//B dominates D, E, E, G
//B doest not dominate H, G, C
//C dominates F
//A dominates H
//G doest not dominate H
TEST(lox_ir_block_dominates){
    struct lox_ir_block * a = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * b = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * c = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * d = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * e = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * f = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * g = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    struct lox_ir_block * h = alloc_lox_ir_block(NATIVE_LOX_ALLOCATOR());
    a->type_next = TYPE_NEXT_LOX_IR_BLOCK_BRANCH;
    a->next_as.branch.true_branch = b;
    a->next_as.branch.false_branch = c;
    b->type_next = TYPE_NEXT_LOX_IR_BLOCK_BRANCH;
    b->next_as.branch.true_branch = d;
    b->next_as.branch.false_branch = e;
    add_u64_set(&b->predecesors, (uint64_t) a);
    d->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    d->next_as.next = g;
    add_u64_set(&d->predecesors, (uint64_t) b);
    e->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    e->next_as.next = g;
    add_u64_set(&e->predecesors, (uint64_t) b);
    g->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    g->next_as.next = h;
    add_u64_set(&g->predecesors, (uint64_t) d);
    add_u64_set(&g->predecesors, (uint64_t) e);
    c->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    c->next_as.next = f;
    add_u64_set(&c->predecesors, (uint64_t) a);
    f->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    f->next_as.next = h;
    add_u64_set(&f->predecesors, (uint64_t) c);
    h->type_next = TYPE_NEXT_LOX_IR_BLOCK_NONE;
    add_u64_set(&h->predecesors, (uint64_t) g);
    add_u64_set(&h->predecesors, (uint64_t) f);

    //Unused in dominates_block_lox_ir
    struct lox_ir * lox_ir = NULL;

    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, b, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, c, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, d, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, e, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, f, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, g, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, a, h, NATIVE_LOX_ALLOCATOR()));

    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, b, d, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, b, e, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, b, g, NATIVE_LOX_ALLOCATOR()));
    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, b, e, NATIVE_LOX_ALLOCATOR()));
    ASSERT_FALSE(dominates_block_lox_ir(lox_ir, b, h, NATIVE_LOX_ALLOCATOR()));

    ASSERT_TRUE(dominates_block_lox_ir(lox_ir, c, f, NATIVE_LOX_ALLOCATOR()))
    ASSERT_FALSE(dominates_block_lox_ir(lox_ir, f, h, NATIVE_LOX_ALLOCATOR()))
}

//((a + b) + c) == (b + (a + c))
TEST(lox_ir_data_node_is_eq) {
    ASSERT_TRUE(is_eq_lox_ir_data_node(create_add_data_node1(), create_add_data_node2(), NATIVE_LOX_ALLOCATOR()));
}

//Hash((a + b) + c) == Hash(b + (a + c))
TEST(lox_ir_data_node_hash) {
    ASSERT_EQ(hash_lox_ir_data_node(create_add_data_node1()), hash_lox_ir_data_node(create_add_data_node2()));
}

//Returns: ((a + b) + c)
static struct lox_ir_data_node * create_add_data_node1() {
    struct lox_ir_data_get_ssa_name_node * get_a = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_a->ssa_name = CREATE_SSA_NAME(1, 1);
    struct lox_ir_data_get_ssa_name_node * get_b = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_b->ssa_name = CREATE_SSA_NAME(2, 1);
    struct lox_ir_data_get_ssa_name_node * get_c = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_c->ssa_name = CREATE_SSA_NAME(3, 1);

    //First tree: (a + b) + c
    struct lox_ir_data_binary_node * first_tree_add_a_b = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    first_tree_add_a_b->right = &get_b->data;
    first_tree_add_a_b->left = &get_a->data;
    first_tree_add_a_b->operator = OP_ADD;
    struct lox_ir_data_binary_node * first_tree_head_node_add_c = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    first_tree_head_node_add_c->right = &get_c->data;
    first_tree_head_node_add_c->left = &first_tree_add_a_b->data;
    first_tree_head_node_add_c->operator = OP_ADD;

    return &first_tree_head_node_add_c->data;
}

//Returns: b + (a + c)
static struct lox_ir_data_node * create_add_data_node2() {
    struct lox_ir_data_get_ssa_name_node * get_a = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_a->ssa_name = CREATE_SSA_NAME(1, 1);
    struct lox_ir_data_get_ssa_name_node * get_b = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_b->ssa_name = CREATE_SSA_NAME(2, 1);
    struct lox_ir_data_get_ssa_name_node * get_c = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    get_c->ssa_name = CREATE_SSA_NAME(3, 1);

    struct lox_ir_data_binary_node * second_tree_add_a_c = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    second_tree_add_a_c->right = &get_c->data;
    second_tree_add_a_c->left = &get_a->data;
    second_tree_add_a_c->operator = OP_ADD;
    struct lox_ir_data_binary_node * second_tree_head_node_add_b = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_BINARY, struct lox_ir_data_binary_node, NULL, NATIVE_LOX_ALLOCATOR()
    );
    second_tree_head_node_add_b->right = &second_tree_add_a_c->data;
    second_tree_head_node_add_b->left = &get_b->data;
    second_tree_head_node_add_b->operator = OP_ADD;

    return &second_tree_head_node_add_b->data;
}