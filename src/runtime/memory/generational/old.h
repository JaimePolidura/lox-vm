#pragma once

#include "runtime/memory/generational/memory_space.h"
#include "runtime/memory/generational/mark_bitmap.h"
#include "runtime/memory/generational/utils.h"

#include "shared/types/types.h"
#include "shared/config/config.h"
#include "shared.h"

struct old {
    struct memory_space memory_space;
    struct mark_bitmap * mark_bitmap;
};

struct old * alloc_old(struct config);

uint8_t * move_to_old(struct old *, struct object * object);

bool belongs_to_old(struct old *, uintptr_t ptr);