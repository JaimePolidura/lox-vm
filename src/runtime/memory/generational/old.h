#pragma once

#include "shared/types/types.h"
#include "shared/config/config.h"
#include "shared.h"

struct old {

};

struct old * alloc_old(struct config);

uint8_t * move_to_old(struct old *, struct object * object);
