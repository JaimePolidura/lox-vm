#include "const_folding_optimization.h"

typedef enum {
    SEMILATTICE_TOP, //Unknown new_local_value
    SEMILATTICE_CONSTANT, //Constant new_local_value
    SEMILATTICE_BOTTON //Multiple values
} semilattice_type_t;

struct semilattice_value {
    semilattice_type_t type;
    lox_value_t const_value; //Only used when type is SEMILATTICE_CONSTANT
};

void perform_const_folding_optimization(struct ssa_block * start_block) {
    start_block = start_block->next.next;
}