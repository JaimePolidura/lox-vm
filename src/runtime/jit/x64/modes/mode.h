#pragma once

//Represents "execution mode" when a thread is executing jit compiled code
//This is typically used when calling external functions in jit compiled code
//jit_mode_t is stored in jit_compiler.h This is only used at jit compilation time
typedef enum {
    //Set when vm_thread is runnig jit compiled code
    //vm_thread esp holds the value before the jit compiled function was run. It is not updated anymore.
    //Stack values inside the jit compiled function are stored in registers.
    //RCX points to vm_thread esp
    //RCD points to callframe slots
    MODE_JIT,

    //Set when vm_thread that is running jit compiled code is going to be expected by a garbage collection.
    //This will construct vm_thread stack with only the heap allocated objects stored in the compile time datastructure jit_stack.
    //The stack will get "deconstructed" when switching back to JIT mode
    //The vm stack is not expected to change during MODE_VM_GC
    MODE_VM_GC,
} jit_mode_t;

