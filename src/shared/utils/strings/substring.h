#pragma once

#include "shared.h"

struct substring {
    char * source;
    int start_inclusive;
    int end_exclusive;
};

char * copy_substring_to_string(struct substring substring);
int length_substring(struct substring substring);
char * start_substring(struct substring substring);
