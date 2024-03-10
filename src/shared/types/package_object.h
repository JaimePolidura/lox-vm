#pragma once

#include "shared.h"
#include "types.h"
#include "shared/package.h"

struct package_object {
    struct object object;
    struct package * package;
};

void init_package_object(struct package_object * package_object);

lox_value_t to_lox_package(struct package * package);