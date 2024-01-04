#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

uint32_t hash_string(const char * string_ptr, int length);

// #define DEBUG_TRACE_EXECUTION