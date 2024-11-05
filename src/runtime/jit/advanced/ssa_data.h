#include "shared/bytecode/bytecode.h"
#include "shared/package.h"
#include "shared.h"
#include "shared/types/struct_definition_object.h"
#include "runtime/profiler/profile_data.h"

//Data flow nodes used in SSA IR

typedef enum {
    SSA_DATA_NODE_TYPE_GET_LOCAL,
    SSA_DATA_NODE_TYPE_SET_LOCAL,
    SSA_DATA_NODE_TYPE_GET_GLOBAL,
    SSA_DATA_NODE_TYPE_SET_GLOBAL,
    SSA_DATA_NODE_TYPE_COMPARATION,
    SSA_DATA_NODE_TYPE_ARITHMETIC,
    SSA_DATA_NODE_TYPE_UNARY,
    SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD,
    SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT,
    SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT,
    SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY,
} ssa_data_node_type;

struct ssa_data {
    ssa_data_node_type type;
};

//OP_SET_LOCAL
struct ssa_data_set_local_instruction {
    struct ssa_data data;

    int local_number;
    struct ssa_data * ssa_data;
};

//OP_GET_LOCAL
struct ssa_data_get_local_instruction {
    struct ssa_data data;

    int local_number;
    profile_data_type_t type;
    int n_phi_numbers_elements;
    int * phi_numbers;
};

//OP_GET_GLOBAL
struct ssa_data_get_global {
    struct ssa_data data;

    struct package * package;
    struct string_object * name;
};

//OP_SET_GLOBAL
struct ssa_data_set_global {
    struct ssa_data data;

    struct package * package;
    struct string_object * name;
    struct ssa_data * value;
};

//OP_EQUAL, OP_LESS, OP_GREATER
struct ssa_data_comparation {
    struct ssa_data data;

    struct ssa_data * left;
    profile_data_type_t left_type;

    struct ssa_data * right;
    profile_data_type_t right_type;

    bytecode_t operand;
};

//OP_ADD, OP_SUB, OP_MUL, OP_DIV
struct ssa_data_arithmetic {
    struct ssa_data data;

    struct ssa_data * left;
    profile_data_type_t left_type;

    struct ssa_data * right;
    profile_data_type_t right_type;

    bytecode_t operand;
};

//OP_CONST, OP_FAST_CONST_8, OP_FAST_CONST_16, OP_CONST_1, OP_CONST_2
struct ssa_data_constant {
    struct ssa_data data;

    lox_value_type type;
    union {
        struct string_object * string;
        struct object * object;
        lox_value_t number;
        bool boolean;
        void * nil;
    } as;
};

//OP_NEGATE, OP_NOT
struct ssa_data_unary {
    struct ssa_data data;

    struct ssa_data * unary_value;
    enum {
        UNARY_OPERATION_TYPE_NOT,
        UNARY_OPERATION_TYPE_NEGATION,
    } unary_operation_type;
};

struct ssa_data_get_struct_field {
    struct ssa_data data;

    struct string_object * field_name;
    struct ssa_data * instance;
};

struct ssa_data_initialize_struct {
    struct ssa_data data;

    struct struct_definition_object * definition;
    struct ssa_data ** fields;
};

struct ssa_data_get_array_element {
    struct ssa_data data;

    int index;
    struct ssa_data * instance;
};

struct ssa_data_initialize_array {
    struct ssa_data data;

    int n_elements;
    bool empty_initialization;
    struct ssa_data ** elememnts;
};
