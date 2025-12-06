#include "shared/types/native_function_object.h"
#include "shared/os/os_utils.h"
#include "shared.h"

static uint64_t clock_jit_native() {
    return time_millis();
}

static lox_value_t clock_vm_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(clock_jit_native());
}

struct native_function_object clock_native_function = (struct native_function_object) {
    .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
    .returned_type = PROFILE_DATA_TYPE_I64,
    .native_jit_fn = clock_jit_native,
    .native_vm_fn = clock_vm_native,
    .name = "clock"
};