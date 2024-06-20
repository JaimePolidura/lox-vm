#pragma once

#include "runtime/memory/generational/mark_bitmap.h"
#include "runtime/memory/generational/memory_space.h"
#include "runtime/memory/generational/utils.h"

#include "shared/config/config.h"
#include "shared/types/types.h"
#include "shared.h"

struct survivor {
    struct mark_bitmap * fromspace_updated_references_mark_bitmap;
    struct mark_bitmap * fromspace_moved_mark_bitmap;

    struct memory_space * from;
    struct memory_space * to;
};

struct survivor * alloc_survivor(struct config);
void swap_from_to_survivor_space(struct survivor *);
bool belongs_to_survivor(struct survivor *, uintptr_t ptr);
uint8_t * move_to_survivor_space(struct survivor *, struct object * object);