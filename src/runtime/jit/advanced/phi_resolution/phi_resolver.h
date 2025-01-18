#pragma once

#include "runtime/jit/advanced/phi_resolution/v_register.h"
#include "runtime/jit/advanced/ssa_ir.h"

#include "shared.h"

void resolve_phi(struct ssa_ir *);