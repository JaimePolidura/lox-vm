#pragma once

#include "test.h"
#include "shared/utils/collections/lox_hash_table.h"
#include "shared/utils/utils.h"

#define STRING_TO_OBJ(string_chars) { \
    .length = strlen(string_chars), \
    .hash = hash_string(string_chars, strlen(string_chars)), \
    .chars = string_chars, \
    .object = { \
        .gc_info = NULL \
    }}

TEST(hash_table_put_if_absent){
    struct lox_hash_table table;
    init_hash_table(&table, NATIVE_LOX_ALLOCATOR());
    struct string_object key1 = STRING_TO_OBJ("key1");

    put_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(1));

    ASSERT_FALSE(put_if_absent_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(2)));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    lox_value_t value = get_hash_table(&table, &key1);
    ASSERT_TRUE(AS_NUMBER(value) == 1);

    remove_hash_table(&table, &key1);

    ASSERT_TRUE(put_if_absent_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(3)));
    ASSERT_TRUE(contains_hash_table(&table, &key1));

    value = get_hash_table(&table, &key1);
    ASSERT_TRUE(AS_NUMBER(value) == 3);
}

TEST(hash_table_put_if_present){
    struct lox_hash_table table;
    init_hash_table(&table, NATIVE_LOX_ALLOCATOR());
    struct string_object key1 = STRING_TO_OBJ("key1");

    ASSERT_FALSE(put_if_present_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(1)));
    ASSERT_FALSE(contains_hash_table(&table, &key1));

    put_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(1));

    ASSERT_TRUE(put_if_present_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(2)));
    ASSERT_TRUE(contains_hash_table(&table, &key1));

    lox_value_t value = get_hash_table(&table, &key1);
    ASSERT_TRUE(AS_NUMBER(value) == 2);
}

TEST(hash_table_multiple_put_test) {
    struct lox_hash_table table;
    init_hash_table(&table, NATIVE_LOX_ALLOCATOR());

    struct string_object key1 = STRING_TO_OBJ("key1");

    put_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(1));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    lox_value_t result = get_hash_table(&table, &key1);
    ASSERT_TRUE(AS_NUMBER(result) == 1);

    put_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(2));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    result = get_hash_table(&table, &key1);
    ASSERT_TRUE(AS_NUMBER(result) == 2);
}

TEST(hash_table_put_contains_remove_get_test) {
    struct lox_hash_table table;
    init_hash_table(&table, NATIVE_LOX_ALLOCATOR());

    struct string_object key1 = STRING_TO_OBJ("key1-1");
    struct string_object key2 = STRING_TO_OBJ("key1");

    put_hash_table(&table, &key1, TO_LOX_VALUE_NUMBER(1));

    ASSERT_TRUE(contains_hash_table(&table, &key1));
    ASSERT_FALSE(contains_hash_table(&table, &key2));

    lox_value_t result = get_hash_table(&table, &key1);
    ASSERT_TRUE(result != NIL_VALUE);
    ASSERT_TRUE(IS_NUMBER(result));
    ASSERT_TRUE(AS_NUMBER(result) == 1);

    put_hash_table(&table, &key2, TO_LOX_VALUE_NUMBER(2));
    ASSERT_TRUE(contains_hash_table(&table, &key1));
    ASSERT_TRUE(contains_hash_table(&table, &key2));

    ASSERT_TRUE(remove_hash_table(&table, &key1));
    ASSERT_FALSE(contains_hash_table(&table, &key1));
    ASSERT_TRUE(contains_hash_table(&table, &key2));
}