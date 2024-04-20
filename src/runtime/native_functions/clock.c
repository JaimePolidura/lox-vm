#include "shared/types/native_function_object.h"
#include "shared/os/os_utils.h"
#include "shared.h"

static lox_value_t clock_native(int n_args, lox_value_t * args) {
    return TO_LOX_VALUE_NUMBER(time_millis());
}

struct native_function_object clock_native_function = (struct native_function_object) {
    .object = LOX_OBJECT(OBJ_NATIVE_FUNCTION),
    .native_fn = clock_native,
    .name = "clock"
};