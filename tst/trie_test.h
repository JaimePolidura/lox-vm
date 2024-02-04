#pragma once

#include "test.h"
#include "utils/trie.h"

TEST(trie_test_free_no_segfault) {
    struct trie_list * trie_list = alloc_trie_list();
    ASSERT_TRUE(put_trie(trie_list, "hola", 4, (void *) 0x01));
    ASSERT_TRUE(put_trie(trie_list, "helicoptero", 11, (void *) 0x02));
    ASSERT_TRUE(put_trie(trie_list, "_caca", 5, (void *) 0x03));
    ASSERT_TRUE(put_trie(trie_list, "AVIONETA", 8, (void *) 0x04));
    ASSERT_TRUE(put_trie(trie_list, "PSOE", 4, (void *) 0x05));

    free_trie_list(trie_list);
}

TEST(trie_test_put_with_general_case) {
    struct trie_list * trie_list = alloc_trie_list();
    ASSERT_TRUE(put_trie(trie_list, "hola", 4, (void *) 0x01));
    ASSERT_TRUE(put_trie(trie_list, "helicoptero", 11, (void *) 0x02));
    ASSERT_TRUE(put_trie(trie_list, "_caca", 5, (void *) 0x03));
    ASSERT_TRUE(put_trie(trie_list, "AVIONETA", 8, (void *) 0x04));
    ASSERT_TRUE(put_trie(trie_list, "PSOE", 4, (void *) 0x05));

    ASSERT_TRUE(find_trie(trie_list, "hola", 4) == (void *) 0x01);
    ASSERT_TRUE(find_trie(trie_list, "helicoptero", 11) == (void *) 0x02);
    ASSERT_TRUE(find_trie(trie_list, "_caca", 5) == (void *) 0x03);
    ASSERT_TRUE(find_trie(trie_list, "AVIONETA", 8) == (void *) 0x04);
    ASSERT_TRUE(find_trie(trie_list, "PSOE", 4) == (void *) 0x05);
}

TEST(trie_test_put_with_same_prefix_key) {
    struct trie_list * trie_list = alloc_trie_list();
    ASSERT_TRUE(put_trie(trie_list, "hola", 4, (void *) 0x01));
    ASSERT_TRUE(put_trie(trie_list, "holas", 5, (void *) 0x02));
    ASSERT_TRUE(put_trie(trie_list, "hostias", 7, (void *) 0x03));
    ASSERT_TRUE(put_trie(trie_list, "holi", 4, (void *) 0x04));

    ASSERT_TRUE(find_trie(trie_list, "hola", 4) == (void *) 0x01);
    ASSERT_TRUE(find_trie(trie_list, "holas", 5) == (void *) 0x02);
    ASSERT_TRUE(find_trie(trie_list, "hostias", 7) == (void *) 0x03);
    ASSERT_TRUE(find_trie(trie_list, "holi", 4) == (void *) 0x04);
}

TEST(trie_test_put_with_same_key) {
    struct trie_list * trie_list = alloc_trie_list();
    ASSERT_TRUE(put_trie(trie_list, "hola", 4, NULL));
    ASSERT_FALSE(put_trie(trie_list, "Hola", 4, NULL));
    ASSERT_FALSE(put_trie(trie_list, "holA", 4, NULL));
    ASSERT_FALSE(put_trie(trie_list, "hola", 4, NULL));
}

TEST(trie_test_emtpy_trie_contains) {
    struct trie_list * trie_list = alloc_trie_list();

    ASSERT_FALSE(contains_trie(trie_list, "llave", 5));
}