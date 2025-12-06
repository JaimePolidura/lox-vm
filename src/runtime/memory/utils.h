#pragma once

#include "shared/types/types.h"
#include "shared/types/struct_instance_object.h"
#include "shared/types/struct_definition_object.h"
#include "shared/types/array_object.h"

int sizeof_heap_allocated_lox_object(struct object *);

void free_heap_allocated_lox_object(struct object *);