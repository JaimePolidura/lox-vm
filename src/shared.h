#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

typedef int lox_thread_id;

uint32_t hash_string(const char * string_ptr, int length);

//#define NAN_BOXING

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 1)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)realloc(pointer, sizeof(type) * (newCount))
