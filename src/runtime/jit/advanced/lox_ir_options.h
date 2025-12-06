#pragma once

#include "shared.h"

enum {
    LOX_IR_CREATION_OPT_DONT_USE_BRANCH_PROFILE = 1, //Will compile untaken branches
    LOX_IR_CREATION_OPT_USE_BRANCH_PROFILE = 1 << 1 //Wont compile untaken branches
};