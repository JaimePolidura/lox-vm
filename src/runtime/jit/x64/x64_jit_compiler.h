#pragma once

#include "runtime/jit/x64/operations/binary_operations.h"
#include "runtime/jit/x64/operations/single_operations.h"
#include "runtime/jit/jit_compilation_result.h"
#include "runtime/jit/x64/register_allocator.h"
#include "runtime/jit/x64/function_calls.h"
#include "runtime/jit/jit_compiler_arch.h"
#include "runtime/jit/x64/jit_stack.h"
#include "runtime/threads/vm_thread.h"
#include "runtime/jit/x64/opcodes.h"
#include "runtime/jit/x64/modes/mode.h"
#include "runtime/jit/x64/jit_stack.h"
#include "registers.h"

#include "runtime/memory/gc_algorithm.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/bytecode/pending_jump_to_resolve.h"
#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "shared/package.h"

#include "shared/bytecode/bytecode.h"

//x64_jit_compiler.c implements the jit compiler for x64
//The functions imeplemented are defined in jit_compiler.h

struct jit_compiler {
    //Current lox function being compiled
    struct function_object * function_to_compile;

    //Next bytecode instruction to compile
    uint8_t * pc;

    //Mapping of bytecode index operations to its compiled operations index stored in native_compiled_code
    //This is used for knowing the relative offset when emitting assembly backward jumps
    //If value_node is -1, the native index will be in the next slot
    uint16_t * compiled_bytecode_to_native_by_index;

    //This datastructe allow us to resolve forward jump offsets
    struct pending_jumps_to_resolve pending_jumps_to_resolve;

    //Keep tracks of the last local slot allocated. Locals will be stored in the stack
    int8_t last_stack_slot_allocated;

    struct register_allocator register_allocator;

    //The native code being compiled
    struct u8_arraylist native_compiled_code;

    //Keep tracks of which package we are compiling. This is used for being able to get the correct global variables
    //It works the same way as vm.h
    struct stack_list package_stack;

    //As our vm is a stack base, when we jit compile the code we need some way to know where the stack values
    //reside, maybe they are in registers, or stored in the native stack, or maybe they are just immediate values.
    //When we jit compile the code we "simulate" the vm stack with jit_stack
    struct jit_stack jit_stack;

    jit_mode_t current_mode;
};

struct pop_stack_operand_result {
    struct operand operand;
    uint8_t instruction_index;
};