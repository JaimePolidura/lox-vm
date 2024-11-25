#pragma once

#include "test.h"
#include "shared/utils/collections/stack_list.h"

TEST(simple_stack_push_pop_test) {
    struct stack_list * stack = alloc_stack_list(NATIVE_LOX_ALLOCATOR());
    push_stack_list(stack, (void *) 0x01);

    ASSERT_TRUE(pop_stack_list(stack) == (void *) 0x01);
    ASSERT_TRUE(is_empty_stack_list(stack));

    push_stack_list(stack, (void *) 0x03);
    push_stack_list(stack, (void *) 0x04);
    push_stack_list(stack, (void *) 0x05);

    ASSERT_TRUE(pop_stack_list(stack) == (void *) 0x05);
    ASSERT_TRUE(pop_stack_list(stack) == (void *) 0x04);

    push_stack_list(stack, (void *) 0x10);

    ASSERT_TRUE(pop_stack_list(stack) == (void *) 0x10);
    ASSERT_TRUE(pop_stack_list(stack) == (void *) 0x03);
    ASSERT_TRUE(pop_stack_list(stack) == NULL);
}

TEST(simple_stack_clear_test) {
    struct stack_list * stack = alloc_stack_list(NATIVE_LOX_ALLOCATOR());
    push_stack_list(stack, (void *) 0x01);
    free_stack_list(stack);
    ASSERT_TRUE(pop_stack_list(stack) == NULL);
}
