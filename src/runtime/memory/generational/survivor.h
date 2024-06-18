#pragma once

#include "shared/types/types.h"
#include "shared/config/config.h"
#include "shared.h"

struct survivor {
    struct survivor_space * from;
    struct survivor_space * to;
};

struct survivor_space {
    lox_value_t * start;
    lox_value_t * end;
    lox_value_t * current;
};

struct survivor * alloc_survivor(struct config);