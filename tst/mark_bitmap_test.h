#pragma once

#include "runtime/memory/generational/mark_bitmap.h"
#include "test.h"

struct mark_bitmap * alloc_mark_bitmap_for_test();

TEST(multiple_mark_bitmap_test){
    struct mark_bitmap * mark_bitmap = alloc_mark_bitmap_for_test();
    uint64_t val = (uint64_t) mark_bitmap->start_address;

    uint8_t * a = mark_bitmap->start_address;
    uint8_t * b = mark_bitmap->start_address + 1 * sizeof(void *);
    uint8_t * c = mark_bitmap->start_address + 2 * sizeof(void *);
    uint8_t * d = mark_bitmap->start_address + 4 * sizeof(void *);
    uint8_t * e = mark_bitmap->start_address + 8 * sizeof(void *);
    uint8_t * f = mark_bitmap->start_address + 10 * sizeof(void *);
    set_marked_bitmap(mark_bitmap, a);
    set_marked_bitmap(mark_bitmap, b);
    set_marked_bitmap(mark_bitmap, c);
    set_marked_bitmap(mark_bitmap, d);
    set_marked_bitmap(mark_bitmap, e);
    set_marked_bitmap(mark_bitmap, f);

    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, a));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, b));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, c));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, d));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, e));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, f));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, a + 3 * sizeof(void *)));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, a + 5 * sizeof(void *)));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, a + 9 * sizeof(void *)));

    set_unmarked_bitmap(mark_bitmap, b);
    set_unmarked_bitmap(mark_bitmap, e);
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, a));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, b));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, c));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, d));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, e));
    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, f));
}

TEST(mark_bitmap_test_simple) {
    struct mark_bitmap * mark_bitmap = alloc_mark_bitmap_for_test();

    void * to_mark_ptr = mark_bitmap->start_address + 8;

    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr));
    set_marked_bitmap(mark_bitmap, to_mark_ptr);

    ASSERT_TRUE(is_marked_bitmap(mark_bitmap, to_mark_ptr));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr - 1 * sizeof(void *)));
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr + 1 * sizeof(void *)));

    set_unmarked_bitmap(mark_bitmap, to_mark_ptr);
    ASSERT_FALSE(is_marked_bitmap(mark_bitmap, to_mark_ptr));
}

struct mark_bitmap * alloc_mark_bitmap_for_test() {
    void * ptr = malloc(1024);
    struct mark_bitmap * map = alloc_mark_bitmap(1024 / 8, ptr);
    return map;
}