#pragma once

#include "runtime/jit/x64/register_allocator.h"
#include "runtime/jit/x64/pending_path_jump.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/x64/function_calls.h"
#include "registers.h"
#include "runtime/threads/vm_thread.h"
#include "runtime/jit/x64/opcodes.h"
#include "runtime/jit/x64/x64_stack.h"
#include "runtime/jit/jit_compiler_arch.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "shared/package.h"

#include "compiler/bytecode.h"

struct jit_compiler {
    //Lox function being compiled
    struct function_object * function_to_compile;

    //Next bytecode instruction to compile
    uint8_t * pc;

    //Mapping of bytecode instructions to its compiled instructions index stored in native_compiled_code
    //This is used for knowing the relative offset when emitting assembly backward jumps
    //If value is -1, the native index will be in the next slot
    uint16_t * compiled_bytecode_to_native_by_index;

    //Mapping of bytecode instructions to pending_path_jump, which will allow us to know if there is some
    //pending previous forward jump assembly offset to patch.
    struct pending_path_jump ** pending_jumps_to_patch;

    //Keep tracks of the last local slot allocated. Locals will be stored in the stack
    int8_t last_stack_slot_allocated;

    struct register_allocator register_allocator;
    
    struct u8_arraylist native_compiled_code;

    //Keep tracks of which package we are compiling. This is used for being able to get the correct global variables
    //It works the same way as vm.h
    struct stack_list package_stack;
};