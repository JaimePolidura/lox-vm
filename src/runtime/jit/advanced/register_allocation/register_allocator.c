#include "register_allocator.h"

void perform_register_allocation(struct lox_ir * lox_ir) {
    struct ptr_arraylist * live_ranges = perform_live_range_analysis(lox_ir);
    struct interference_graph interference_graph = build_interference_graph(lox_ir, live_ranges);
}