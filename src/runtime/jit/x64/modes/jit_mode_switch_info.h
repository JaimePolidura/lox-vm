#pragma once

#define NO_MODE_SWITCH_INFO (struct jit_mode_switch_info) {}

//Sometimes it is neccesary to store compile-time information when changing from one mode to another mode
struct  jit_mode_switch_info {
    union {
        struct {
            //Stores the number of items that was pushed to the vm_thread stack when it was reconstructed
            //Used to avoid stack overflow, when switch_jit_to_vm_mode is called multiple times
            int stack_grow;
        } jit_to_vm;
    } as;
};