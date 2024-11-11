#include "shared/utils/collections/u8_set.h"
#include "test.h"

//a - b Expect:
TEST(u8_set_test_difference){
    struct u8_set a;
    init_u8_set(&a);
    add_u8_set(&a, 2);
    add_u8_set(&a, 3);

    struct u8_set b;
    init_u8_set(&b);
    add_u8_set(&b, 1);
    add_u8_set(&b, 2);
    add_u8_set(&b, 200);

    difference_u8_set(&a, b);
    ASSERT_EQ(size_u8_set(a), 1);
    ASSERT_TRUE(contains_u8_set(&a, 3));
}

TEST(u8_set_test_union){
    struct u8_set a;
    init_u8_set(&a);

    add_u8_set(&a, 1);
    add_u8_set(&a, 3);
    add_u8_set(&a, 4);
    add_u8_set(&a, 6);

    struct u8_set b;
    init_u8_set(&b);

    add_u8_set(&b, 1);
    add_u8_set(&b, 2);
    add_u8_set(&b, 5);
    add_u8_set(&b, 6);
    add_u8_set(&b, 7);

    union_u8_set(&a, b);

    ASSERT_EQ(size_u8_set(a), 7);
    ASSERT_TRUE(contains_u8_set(&a, 1));
    ASSERT_TRUE(contains_u8_set(&a, 2));
    ASSERT_TRUE(contains_u8_set(&a, 3));
    ASSERT_TRUE(contains_u8_set(&a, 4));
    ASSERT_TRUE(contains_u8_set(&a, 5));
    ASSERT_TRUE(contains_u8_set(&a, 6));
    ASSERT_TRUE(contains_u8_set(&a, 7));
}

TEST(u8_set_test_add_contains){
    struct u8_set set;
    init_u8_set(&set);

    add_u8_set(&set, 1);
    add_u8_set(&set, 2);
    add_u8_set(&set, 1);
    add_u8_set(&set, 4);
    add_u8_set(&set, 78);
    add_u8_set(&set, 127);
    add_u8_set(&set, 254);

    ASSERT_TRUE(contains_u8_set(&set, 1));
    ASSERT_TRUE(contains_u8_set(&set, 2));
    ASSERT_TRUE(contains_u8_set(&set, 4));
    ASSERT_TRUE(contains_u8_set(&set, 78));
    ASSERT_TRUE(contains_u8_set(&set, 127));
    ASSERT_TRUE(contains_u8_set(&set, 254));

    ASSERT_FALSE(contains_u8_set(&set, 3));
    ASSERT_FALSE(contains_u8_set(&set, 128));
    ASSERT_FALSE(contains_u8_set(&set, 12));

    ASSERT_EQ(size_u8_set(set), 6);
}

TEST(u8_set_test_remove){
    struct u8_set set;
    init_u8_set(&set);

    add_u8_set(&set, 1);
    add_u8_set(&set, 2);
    add_u8_set(&set, 1);
    add_u8_set(&set, 4);
    add_u8_set(&set, 78);
    add_u8_set(&set, 127);
    add_u8_set(&set, 254);

    remove_u8_set(&set, 1);
    ASSERT_FALSE(contains_u8_set(&set, 1));
    ASSERT_EQ(size_u8_set(set), 5);

    remove_u8_set(&set, 3);
    ASSERT_EQ(size_u8_set(set), 5);

    remove_u8_set(&set, 128);
    ASSERT_TRUE(contains_u8_set(&set, 127));
    ASSERT_EQ(size_u8_set(set), 5);

    remove_u8_set(&set, 254);
    ASSERT_FALSE(contains_u8_set(&set, 254));
    ASSERT_EQ(size_u8_set(set), 4);
}