#pragma once

//Represents "execution mode" when a thread is executing jit compiled code
//This is typically used when calling external functions in jit compiled code
//jit_mode_t is stored in jit_compiler.h This is only used at jit compilation time
typedef enum {
    //Set when vm_thread is runnig jit compiled code
    //vm_thread esp holds the value before the jit compiled function was run. It is not updated anymore.
    //Stack values inside the jit compiled function are stored in registers.
    //RSP register points to vm_thread's esp pointer before the function was run
    //RBP register points to call frame slots (setup_call_frame_function in vm.c)
    //The stack grows to higher addresses
    MODE_JIT,

    //Set when a vm_thread that is running jit compiled code, needs to call an already compiled C function,
    //As MODE_JIT stack grows to higher address and native stack to lower address, we need some way to "switch" between these modes
    //and store the previous values. Defined in x64_jit_compiler_modes.h
    MODE_NATIVE,

    //Set when a vm_thread that is running jit compiled code, needs to call an already compiled C function that needs
    //access vm_thread's most up-to-date stack data structures (only for reads not for writes)
    //For example when calling safe_point (a gc thread might start marking the caller's vm_thread stack)
    //MODE_VM is a subset of MODE_NATIVE, the only difference is that MODE_NATIVE vm_threads holds the most up-to-date stack
    MODE_VM,
} jit_mode_t;

