#pragma once

#include "shared.h"

enum {
    SSA_CREATION_OPT_DONT_USE_BRANCH_PROFILE = 1, //Will compile untaken branches
    SSA_CREATION_OPT_USE_BRANCH_PROFILE = 1 << 1 //Wont compile untaken branches
};