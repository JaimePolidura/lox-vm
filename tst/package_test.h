#pragma once

#include "shared/package.h"
#include "test.h"

TEST(package_read_package_name_test){
    struct substring package_name = read_package_name("src/math/math.lox", strlen("src/math/math.lox"));
    char * string = copy_substring_to_string(package_name);
    ASSERT_STRING_EQ(string, "math");

    struct substring package_name2 = read_package_name("math.lox", strlen("math.lox"));
    char * string2 = copy_substring_to_string(package_name2);
    ASSERT_STRING_EQ(string2, "math");

    struct substring package_name3 = read_package_name("math", strlen("math"));
    char * string3 = copy_substring_to_string(package_name3);
    ASSERT_STRING_EQ(string3, "math");
}

extern const char * compiling_base_dir;

TEST(package_import_name_to_absolute_path_test) {
    compiling_base_dir = "/home/jaime/my_project";
    char * absolute_path = import_name_to_absolute_path("src/utils/math.lox", strlen("src/utils/math.lox"));
    ASSERT_STRING_EQ(absolute_path, "/home/jaime/my_project/src/utils/math.lox");
}