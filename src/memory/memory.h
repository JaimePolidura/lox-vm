#pragma once

#include "../shared.h"

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity << 1)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)realloc(pointer, sizeof(type) * (newCount))
