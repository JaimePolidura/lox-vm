#pragma once

#include "test.h"
#include "shared/utils/collections/u64_hash_table.h"

TEST(u64_hash_table_grow) {
    struct u64_hash_table table;
    init_u64_hash_table(&table, NATIVE_LOX_ALLOCATOR());
    for(int i = 0; i < 17; i++){
        put_u64_hash_table(&table, (i + 1), (void *) (i + 1));
    }

    for(int i = 0; i < 17; i++){
        ASSERT_TRUE(contains_u64_hash_table(&table, (i + 1)));
    }
}

TEST(u64_hash_table_put_get_test) {
    struct u64_hash_table table;
    init_u64_hash_table(&table, NATIVE_LOX_ALLOCATOR());

    put_u64_hash_table(&table, 1, (void *) 1);
    put_u64_hash_table(&table, 2, (void *) 2);
    put_u64_hash_table(&table, 3, (void *) 3);

    ASSERT_EQ((int) get_u64_hash_table(&table, 1), 1);
    ASSERT_EQ((int) get_u64_hash_table(&table, 2), 2);
    ASSERT_EQ((int) get_u64_hash_table(&table, 3), 3);
    ASSERT_EQ((int) get_u64_hash_table(&table, 4), 0);
}

TEST(u64_hash_table_put_contains_test) {
    struct u64_hash_table table;
    init_u64_hash_table(&table, NATIVE_LOX_ALLOCATOR());

    put_u64_hash_table(&table, 1, (void *) 1);

    ASSERT_TRUE(contains_u64_hash_table(&table, 1));
    ASSERT_FALSE(contains_u64_hash_table(&table, 2));
}

TEST(u64_hash_table_emtpy_iterator){
    struct u64_hash_table table;
    init_u64_hash_table(&table, NATIVE_LOX_ALLOCATOR());
    struct u64_hash_table_iterator iterator;
    init_u64_hash_table_iterator(&iterator, table);

    ASSERT_FALSE(has_next_u64_hash_table_iterator(iterator));
}

TEST(u64_hash_table_iterator){
    struct u64_hash_table table;
    init_u64_hash_table(&table, NATIVE_LOX_ALLOCATOR());
    put_u64_hash_table(&table, 1, (void *) 1);
    put_u64_hash_table(&table, 2, (void *) 2);
    put_u64_hash_table(&table, 3, (void *) 3);
    struct u64_hash_table_iterator iterator;
    init_u64_hash_table_iterator(&iterator, table);

    ASSERT_TRUE(has_next_u64_hash_table_iterator(iterator));
    ASSERT_EQ(next_u64_hash_table_iterator(&iterator).key, 1);

    ASSERT_TRUE(has_next_u64_hash_table_iterator(iterator));
    ASSERT_EQ(next_u64_hash_table_iterator(&iterator).key, 2);

    ASSERT_TRUE(has_next_u64_hash_table_iterator(iterator));
    ASSERT_EQ(next_u64_hash_table_iterator(&iterator).key, 3);

    ASSERT_FALSE(has_next_u64_hash_table_iterator(iterator));
}
