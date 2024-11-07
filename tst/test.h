#pragma once

#include "../src/shared.h"

#define TEST(name) \
    void name##_inner(char * test_name, int n_assertions, int last_offset_vm_log); \
    void name##_outer() { \
        name##_inner(#name, 0, 0); \
        printf("[%s] Success\n", #name); \
    } \
    void name##_inner(char * test_name, int n_assertions, int last_offset_vm_log)

#define ASSERT_NEXT_VM_LOG(vm, expected) \
    do {                                 \
        n_assertions++; \
        if(last_offset_vm_log >= vm.log_entries_in_use) { \
            fprintf(stderr, "[%s] Invalid runtime log assert immediate %i Max log entries have been reached\n", test_name, n_assertions); \
            exit(65); \
        } \
        if(strcmp(vm.log[last_offset_vm_log++], expected) != 0) { \
            fprintf(stderr, "[%s] Invalid runtime log assert immediate %i at log index %i Actual: %s Expected: %s\n", test_name, n_assertions, last_offset_vm_log, vm.log[last_offset_vm_log - 1], expected); \
            exit(65); \
        } \
    }while(false); \

#define ASSERT_STRING_EQ(a, b) \
    do { \
        n_assertions++; \
        if(strcmp(a, b) != 0) { \
            fprintf(stderr, "[%s] Invalid string equals assert immediate %i while comparing %s and %s\n", test_name, n_assertions, a, b); \
            exit(65); \
        } \
    }while(false);

#define ASSERT_EQ(a, b) \
    do { \
        n_assertions++; \
        if(a != b) { \
            fprintf(stderr, "[%s] Invalid equals assert immediate %i while comparing %d and %d\n", test_name, n_assertions, a, b); \
            exit(65); \
        } \
    }while(false); \

#define ASSERT_U8_SEQ(actual, ...) \
    do {                                 \
        n_assertions++; \
        uint8_t bytecode_expected[] = {__VA_ARGS__}; \
        size_t n_bytecode_expected = sizeof(bytecode_expected); \
        for(int i = 0; i < n_bytecode_expected; i++){ \
            if(actual[i] != bytecode_expected[i]) { \
                fprintf(stderr, "[%s] Invalid pending_bytecode equals assert immediate %i while comparing pending_bytecode at index %i Expected %i actual %i\n", test_name, n_assertions, i, bytecode_expected[i], actual[i]); \
                exit(65); \
            } \
        } \
    }while(false); \

#define ASSERT_TRUE(a) \
    do{ \
        n_assertions++; \
        if(!(a)) { \
            fprintf(stderr, "[%s] Invalid true assert at immediate %i\n", test_name, n_assertions); \
            exit(65); \
        } \
    }while(false);

#define ASSERT_FALSE(a) \
    do { \
        n_assertions++; \
        if(a) { \
            fprintf(stderr, "[%s] Invalid false assert at immediate %i\n", test_name, n_assertions); \
            exit(65); \
        } \
    }while(false);

extern struct trie_list * compiled_packages;
extern const char * compiling_base_dir;

void reset_vm() {
    compiling_base_dir = NULL;
    compiled_packages = NULL;
}