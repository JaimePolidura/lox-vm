#pragma once

#define NO_MODE_SWITCH_INFO (struct jit_mode_switch_info) {}

//Sometimes it is neccesary to store compile-time information when changing from one mode to another mode
struct jit_mode_switch_info {
    union {
        struct {
            //Stores the number of items that was pushed to the vm_thread stack when it was reconstructed
            //Used value_as a little optimization when deconstructing vm stack to avoid an extra instruction
            int stack_grow;
        } jit_to_vm_gc;
    } as;
};