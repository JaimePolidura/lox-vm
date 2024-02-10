#include "utils/utils.h"

#include "test.h"

TEST(utils_to_upper_case_test) {
    char * to_test1 = "holA soy Jaime_";
    char * expected1 = "HOLA SOY JAIME_";
    ASSERT_STRING_EQ(expected1, to_upper_case(to_test1, strlen(to_test1)));

    char * expected2 = "HOLA SOY JAIME_";
    ASSERT_STRING_EQ(expected2, to_upper_case(expected2, strlen(expected2)));
}

TEST(utils_string_contains_test){
    ASSERT_TRUE(string_contains("hola", 4, 'h'));
    ASSERT_TRUE(string_contains("adios_", 6, '_'));
    ASSERT_FALSE(string_contains("hola", 4, '5'));
}

TEST(utils_string_equals_ignore_case_test){
    ASSERT_TRUE(string_equals_ignore_case("hola", "Hola", 4));
    ASSERT_TRUE(string_equals_ignore_case("hola", "HOLA", 4));
    ASSERT_TRUE(string_equals_ignore_case("hola_", "HOLA_", 5));
    ASSERT_FALSE(string_equals_ignore_case("hols_", "hola", 4));
}