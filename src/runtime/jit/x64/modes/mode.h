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

    //Set when a vm_thread that is running jit compiled code, needs to call an already compiled C function that needs
    //access vm_thread's most up-to-date stack data structures (only for reads not for writes)
    //For example when calling safe_point (a gc thread might start marking the caller's vm_thread stack)
    //When changing from MODE_JIT to MODE_VM, The vm_thread's stack needs to be reconstructed
    MODE_VM,
} jit_mode_t;

