#pragma once

#include "test.h"
#include "shared/utils/collections/stack_list.h"

TEST(simple_stack_push_pop_test) {
    struct stack_list * stack = alloc_stack_list();
    push_stack(stack, (void *) 0x01);

    ASSERT_TRUE(pop_stack(stack) == (void *) 0x01);
    push_stack(stack, (void *) 0x03);
    push_stack(stack, (void *) 0x04);
    push_stack(stack, (void *) 0x05);

    ASSERT_TRUE(pop_stack(stack) == (void *) 0x05);
    ASSERT_TRUE(pop_stack(stack) == (void *) 0x04);

    push_stack(stack, (void *) 0x10);

    ASSERT_TRUE(pop_stack(stack) == (void *) 0x10);
    ASSERT_TRUE(pop_stack(stack) == (void *) 0x03);
    ASSERT_TRUE(pop_stack(stack) == NULL);
}

TEST(simple_stack_clear_test) {
    struct stack_list * stack = alloc_stack_list();
    push_stack(stack, (void *) 0x01);
    clear_stack(stack);
    ASSERT_TRUE(pop_stack(stack) == NULL);
}
