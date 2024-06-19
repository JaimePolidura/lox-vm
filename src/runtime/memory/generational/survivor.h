#pragma once

#include "runtime/memory/generational/mark_bitmap.h"

#include "shared/config/config.h"
#include "shared/types/types.h"
#include "shared.h"

struct survivor {
    struct survivor_space * from;
    struct survivor_space * to;
};

struct survivor_space {
    struct mark_bitmap * mark_bitmap;

    uint8_t * start;
    uint8_t * end;
    uint8_t * current;
};

struct survivor * alloc_survivor(struct config);
void swap_from_to_survivor_space(struct survivor *);
bool belongs_to_survivor(struct survivor *, uintptr_t ptr);
uint8_t * move_to_survivor_space(struct survivor_space *, struct object * object);