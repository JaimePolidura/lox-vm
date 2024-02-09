#pragma once

#include "shared.h"

#define TO_UPPER_CASE(character) ( ~((character >= 'a' & character <= 'z') << 5) & character )

char * to_upper_case(char * key, int length);
bool string_contains(char * string, int length, char to_check);
bool string_equals_ignore_case(char * a, char * b, int length_a);