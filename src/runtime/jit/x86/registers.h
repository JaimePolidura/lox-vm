#pragma once

#define N_MAX_REGISTERS 16

typedef enum {
    RAX =  0, // 0000
    RCX =  1, // 0001
    RDX =  2, // 0010
    RBX =  3, // 0011
    RSP =  4, // 0100
    RBP =  5, // 0101
    RSI =  6, // 0110
    RDI =  7, // 0111
    R8 =   8, // 1000
    R9 =   9, // 1001
    R10 = 10, // 1010
    R11 = 11, // 1101
    R12 = 12, // 1100
    R13 = 13, // 1101
    R14 = 14, // 1110
    R15 = 15  // 1111
} register_t;

//0111
#define TO_32_BIT_REGISTER(reg) reg & 0x07