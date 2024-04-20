#include "native_function_definer.h"

extern struct native_function_object self_thread_id_native_function;
extern struct native_function_object force_gc_native_function;
extern struct native_function_object clock_native_function;
extern struct native_function_object join_native_function;
extern struct native_function_object sleep_native_function;

extern __thread struct vm_thread * self_thread;
static void define_native(struct native_function_object * native_function_desc);

void setup_native_functions() {
    define_native(&self_thread_id_native_function);
    define_native(&force_gc_native_function);
    define_native(&sleep_native_function);
    define_native(&clock_native_function);
    define_native(&join_native_function);
}

static void define_native(struct native_function_object * native_function_object) {
    put_hash_table(&self_thread->current_package->global_variables,
                   add_to_global_string_pool(native_function_object->name, strlen(native_function_object->name))
                        .string_object,
                   TO_LOX_VALUE_OBJECT(native_function_object));
}
