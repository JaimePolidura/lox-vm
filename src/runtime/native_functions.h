#pragma once

#include "runtime/threads/vm_thread.h"
#include "shared/types/native_object.h"
#include "shared/os/os_utils.h"
#include "runtime/memory/gc.h"

lox_value_t clock_native(int n_args, lox_value_t * args);
lox_value_t join_native(int n_args, lox_value_t * args);
lox_value_t sleep_ms_native(int n_args, lox_value_t * args);
lox_value_t self_thread_id_native(int n_args, lox_value_t * args);
lox_value_t force_gc(int n_args, lox_value_t * args);