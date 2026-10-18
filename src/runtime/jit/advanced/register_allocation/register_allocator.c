#include "register_allocator.h"

void perform_register_allocation(struct lox_ir * lox_ir) {
    struct ptr_arraylist * live_ranges = perform_live_range_analysis(lox_ir);
}