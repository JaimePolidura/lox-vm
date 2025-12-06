#pragma once

#include "runtime/memory/generational/mark_bitmap.h"
#include "runtime/memory/generational/memory_space.h"
#include "runtime/memory/generational/card_table.h"
#include "runtime/memory/generational/utils.h"

#include "shared/config/config.h"
#include "shared/types/types.h"
#include "shared.h"

struct survivor {
    //Used in marking & updating references
    struct mark_bitmap fromspace_mark_bitmap;
    struct card_table fromspace_card_table;

    struct memory_space * from;
    struct memory_space * to;
};

struct survivor * alloc_survivor(struct config);
void swap_from_to_survivor_space(struct survivor *, struct config);
bool belongs_to_survivor(struct survivor *, uintptr_t ptr);
uint8_t * move_to_survivor_space(struct survivor *, struct object * object);