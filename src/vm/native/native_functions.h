#pragma once

#include "vm/threads/vm_thread.h"
#include "types/native_object.h"
#include "utils/os/os_utils.h"
#include "memory/gc/gc.h"

lox_value_t clock_native(int n_args, lox_value_t * args);
lox_value_t join_native(int n_args, lox_value_t * args);
lox_value_t sleep_ms_native(int n_args, lox_value_t * args);
lox_value_t self_thread_id_native(int n_args, lox_value_t * args);
lox_value_t force_gc(int n_args, lox_value_t * args);