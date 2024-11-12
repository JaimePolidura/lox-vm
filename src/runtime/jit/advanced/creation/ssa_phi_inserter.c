#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/types/function_object.h"
#include "shared/package.h"

struct ssa_control_node * create_ssa_ir_no_phis(
        struct package * package,
        struct function_object * function,
        struct ssa_block * start_block,
        struct ssa_control_node * start_node
) {
    start_block = start_block->next.next;
    start_node = start_node->next.next;

    return NULL;
}