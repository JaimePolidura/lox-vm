#pragma once

#include "test.h"
#include "../src/table/table.h"

#define STRING_TO_OBJ(string_chars) { \
    .length = strlen(string_chars), \
    .hash = hash_string(string_chars, strlen(string_chars)), \
    .chars = string_chars, \
    .object = { \
        .next = NULL, \
        .type = OBJ_STRING \
    }}

TEST(hash_table_multiple_put_test) {
    struct hash_table table;
    init_hash_table(&table);

    struct string_object key1 = STRING_TO_OBJ("key1");

    put_hash_table(&table, &key1, FROM_RAW_TO_NUMBER(1));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    lox_value_t result;
    get_hash_table(&table, &key1, &result);
    ASSERT_TRUE((result.as.number == 1));

    put_hash_table(&table, &key1, FROM_RAW_TO_NUMBER(2));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    get_hash_table(&table, &key1, &result);
    ASSERT_TRUE((result.as.number == 2));
}

TEST(hash_table_put_contains_remove_get_test) {
    struct hash_table table;
    init_hash_table(&table);

    struct string_object key1 = STRING_TO_OBJ("key1-1");
    struct string_object key2 = STRING_TO_OBJ("key1");

    put_hash_table(&table, &key1, FROM_RAW_TO_NUMBER(1));

    ASSERT_TRUE(contains_hash_table(&table, &key1));
    ASSERT_FALSE(contains_hash_table(&table, &key2));

    lox_value_t result;
    ASSERT_TRUE(get_hash_table(&table, &key1, &result));
    ASSERT_EQ(result.type, VAL_NUMBER);
    ASSERT_TRUE((result.as.number == 1));

    put_hash_table(&table, &key2, FROM_RAW_TO_NUMBER(2));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    ASSERT_TRUE(contains_hash_table(&table, &key2));

    ASSERT_TRUE(remove_hash_table(&table, &key1));
    ASSERT_FALSE(contains_hash_table(&table, &key1));
    ASSERT_TRUE(contains_hash_table(&table, &key2));
}

TEST(hash_table_add_all_test) {
    struct hash_table tableA;
    init_hash_table(&tableA);

    struct hash_table tableB;
    init_hash_table(&tableB);

    struct string_object key1 = STRING_TO_OBJ("key1");
    struct string_object key2 = STRING_TO_OBJ("key2");
    struct string_object key3 = STRING_TO_OBJ("key3");
    struct string_object key4 = STRING_TO_OBJ("key4");

    put_hash_table(&tableA, &key1, FROM_RAW_TO_NUMBER(1));
    put_hash_table(&tableA, &key2, FROM_RAW_TO_NUMBER(2));
    put_hash_table(&tableA, &key3, FROM_RAW_TO_NUMBER(3));

    put_hash_table(&tableB, &key4, FROM_RAW_TO_NUMBER(4));

    add_all_hash_table(&tableA, &tableB);

    contains_hash_table(&tableA, &key1);

    ASSERT_TRUE(contains_hash_table(&tableA, &key1));
    ASSERT_TRUE(contains_hash_table(&tableA, &key2));
    ASSERT_TRUE(contains_hash_table(&tableA, &key3));
    ASSERT_TRUE(contains_hash_table(&tableA, &key4));
}
