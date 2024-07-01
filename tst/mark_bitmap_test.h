#pragma once

#include "runtime/memory/generational/mark_bitmap.h"
#include "test.h"

struct mark_bitmap * alloc_mark_bitmap_for_test();

TEST(mark_bitmap_test) {
    struct mark_bitmap * mark_bitmap = alloc_mark_bitmap_for_test();

    void * to_mark_ptr = mark_bitmap->start_address + 8;

    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr));
    set_marked_bitmap(mark_bitmap, to_mark_ptr);

    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, to_mark_ptr));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr - 1));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr + 1));

    set_unmarked_bitmap(mark_bitmap, to_mark_ptr);
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr));
}

struct mark_bitmap * alloc_mark_bitmap_for_test() {
    void * ptr = malloc(1024);
    struct mark_bitmap * map = alloc_mark_bitmap(1024 / 8, ptr);
    return map;
}