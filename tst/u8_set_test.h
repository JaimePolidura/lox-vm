#include "shared/utils/collections/u8_set.h"
#include "test.h"

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