#pragma once

#include "shared/utils/collections/u8_hash_table.h"
#include "test.h"

TEST(u8_hash_table_test_add_contains){
    struct u8_hash_table table;
    init_u8_hash_table(&table);

    put_u8_hash_table(&table, 1, (void *) 0x01);
    put_u8_hash_table(&table, 5, (void *) 0x02);
    put_u8_hash_table(&table, 7, (void *) 0x03);
    put_u8_hash_table(&table, 7, (void *) 0x04);

    ASSERT_EQ(table.size, 3);
    ASSERT_EQ(get_u8_hash_table(&table, 1), (void *) 0x01);
    ASSERT_EQ(get_u8_hash_table(&table, 5), (void *) 0x02);
    ASSERT_EQ(get_u8_hash_table(&table, 7), (void *) 0x04);
    ASSERT_EQ(get_u8_hash_table(&table, 2), NULL);

    ASSERT_TRUE(contains_u8_hash_table(&table, 1));
    ASSERT_TRUE(contains_u8_hash_table(&table, 5));
    ASSERT_TRUE(contains_u8_hash_table(&table, 7));
    ASSERT_FALSE(contains_u8_hash_table(&table, 2));
}