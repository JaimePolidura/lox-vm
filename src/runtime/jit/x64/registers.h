#pragma once

#define N_MAX_REGISTERS 16

/**
 * RAX used for:
 * - Storing the division result,
 * - Storing in low bytes AL the boolean result of a comparation
 * - Storing the return value of external C function calls
 *
 * RCX used for storing the previous RSP register
 * RDX used for storing the previous RBP register
 *
 * RBP, RSP used for stack managment
 * RSI to R15 are used for register allocation
*/
typedef enum {
    RAX =  0, // 0000
    RCX =  1, // 0001
    RDX =  2, // 0010
    RBX =  3, // 0011
    RSP =  4, // 0100
    RBP =  5, // 0101
    RSI =  6, // 0110
    RDI =  7, // 0111
    R8 =   8, // 1000 Equivalent RAX
    R9 =   9, // 1001 Equivalent RCX
    R10 = 10, // 1010 Equivalent RDX
    R11 = 11, // 1101 Equivalent RBX
    R12 = 12, // 1100 Equivalent RSP
    R13 = 13, // 1101 Equivalent RBP
    R14 = 14, // 1110 Equivalent RSI
    R15 = 15  // 1111 Equivalent RDI
} register_t;

//0111
#define TO_32_BIT_REGISTER(reg) (reg & 0x07)