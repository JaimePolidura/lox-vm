//Control flow nodes used in SSA IR

#include "ssa_data.h"

typedef enum {
    SSA_CONTROL_NODE_TYPE_RETURN,
    SSA_CONTROL_NODE_TYPE_PRINT
} ssa_control_node_type;

struct ssa_control {
    ssa_control_node_type type;
};

struct ssa_control_print {
    struct ssa_data * data;
};

struct ssa_control_return {
    struct ssa_data * data;
};

OP_JUMP_IF_FALSE,            // Index: 22
OP_JUMP,                     // Index: 23
OP_LOOP,                     // Index: 24
OP_CALL,                     // Index: 25
OP_SET_STRUCT_FIELD,         // Index: 28 WB
OP_ENTER_PACKAGE,            // Index: 29
OP_EXIT_PACKAGE,             // Index: 30
OP_ENTER_MONITOR,            // Index: 31
OP_EXIT_MONITOR,             // Index: 32
OP_SET_ARRAY_ELEMENT,        // Index: 35 WB
OP_ENTER_MONITOR_EXPLICIT,   // Index: 41
OP_EXIT_MONITOR_EXPLICIT,    // Index: 42