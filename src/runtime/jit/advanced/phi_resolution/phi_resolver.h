#pragma once

#include "runtime/jit/advanced/phi_resolution/v_register.h"
#include "runtime/jit/advanced/lox_ir.h"

#include "shared.h"

void resolve_phi(struct lox_ir *);