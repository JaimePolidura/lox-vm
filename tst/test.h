#pragma once

#include "../src/shared.h"

#define TEST(name) \
    void name##_inner(char * test_name, int n_assertions); \
    void name##_outer() { \
        name##_inner(#name, 0); \
        printf("[%s] Success\n", #name); \
    } \
    void name##_inner(char * test_name, int n_assertions)

#define ASSERT_EQ(a, b) \
    do { \
        n_assertions++; \
        if(a != b) { \
            fprintf(stderr, "[%s] Invalid equals assert number %i while comparing %d and %d\n", test_name, n_assertions, a, b); \
            exit(65); \
        } \
    }while(false); \

#define ASSERT_BYTECODE_SEQ(actual, ...) \
    do {                                 \
        n_assertions++; \
        uint8_t bytecode_expected[] = {__VA_ARGS__}; \
        size_t n_bytecode_expected = sizeof(bytecode_expected); \
        for(int i = 0; i < n_bytecode_expected; i++){ \
            if(actual[i] != bytecode_expected[i]) { \
                fprintf(stderr, "[%s] Invalid bytecode equals assert number %i while comparing bytecode at index %i Expected %i actual %i\n", test_name, n_assertions, i, bytecode_expected[i], actual[i]); \
                exit(65); \
            } \
        } \
    }while(false); \

#define ASSERT_TRUE(a) \
    do{ \
        n_assertions++; \
        if(!(a)) { \
            fprintf(stderr, "[%s] Invalid true assert at number %i\n", test_name, n_assertions); \
            exit(65); \
        } \
    }while(false);

#define ASSERT_FALSE(a) \
    do { \
        n_assertions++; \
        if(a) { \
            fprintf(stderr, "[%s] Invalid false assert at number %i\n", test_name, n_assertions); \
            exit(65); \
        } \
    }while(false);