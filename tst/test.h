#pragma once

#include "../src/shared.h"

#define ASSERT_EQ(a, b) \
    do { \
        if(a != b) {    \
            fprintf(stderr, "Invalid equals assert while comparing %d and %d\n", a, b); \
            exit(65); \
        } \
    }while(false);                    \

#define ASSERT_STRING_EQ(a, b) \
    do { \
        if(strcmp(a, b) != 0) {\
            fprintf(stderr, "Invalid string equals assert assert while comparing %s and %s\n", a, b); \
            exit(65); \
        } \
    }while(false);

#define ASSERT_TRUE(a) \
    do{ \
        if(!a) {       \
            fprintf(stderr, "Invalid true assert %d\n", a); \
            exit(65); \
        } \
    }while(false);

#define ASSERT_FALSE(a) \
    do { \
        if(a){          \
            fprintf(stderr, "Invalid false assert %d\n", a); \
            exit(65); \
        }                    \
    }while(false); \