#pragma once

#include "test.h"
#include "shared/utils/collections/u64_hash_table.h"

TEST(u64_hash_table_put_get_test) {
    struct u64_hash_table table;
    init_u64_hash_table(&table);

    put_u64_hash_table(&table, 1, (void *) 1);
    put_u64_hash_table(&table, 2, (void *) 2);
    put_u64_hash_table(&table, 3, (void *) 3);

    ASSERT_EQ(get_u64_hash_table(&table, 1), 1);
    ASSERT_EQ(get_u64_hash_table(&table, 2), 2);
    ASSERT_EQ(get_u64_hash_table(&table, 3), 3);
    ASSERT_EQ(get_u64_hash_table(&table, 4), 0);
}

TEST(u64_hash_table_put_contains_test) {
    struct u64_hash_table table;
    init_u64_hash_table(&table);

    put_u64_hash_table(&table, 1, (void *) 1);

    ASSERT_TRUE(contains_u64_hash_table(&table, 1));
    ASSERT_FALSE(contains_u64_hash_table(&table, 2));
}