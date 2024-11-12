#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/struct_definition_object.h"
#include "shared/utils/collections/u8_set.h"
#include "shared/bytecode/bytecode.h"
#include "shared/package.h"
#include "shared.h"

#include "runtime/profiler/profile_data.h"

//Data flow nodes used in SSA IR

typedef enum {
    SSA_DATA_NODE_TYPE_CALL,
    SSA_DATA_NODE_TYPE_GET_LOCAL,
    SSA_DATA_NODE_TYPE_GET_GLOBAL,
    SSA_DATA_NODE_TYPE_COMPARATION,
    SSA_DATA_NODE_TYPE_ARITHMETIC,
    SSA_DATA_NODE_TYPE_CONSTANT,
    SSA_DATA_NODE_TYPE_UNARY,
    SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD,
    SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT,
    SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT,
    SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY,
} ssa_data_node_type;

struct ssa_data_node {
    struct bytecode_list * original_bytecode;
    ssa_data_node_type type;
};

#define ALLOC_SSA_DATA_NODE(type, struct_type, bytecode) (struct_type *) allocate_ssa_data_node(type, sizeof(struct_type), bytecode)

void * allocate_ssa_data_node(ssa_data_node_type type, size_t struct_size_bytes, struct bytecode_list *);
profile_data_type_t get_produced_type_ssa_data(struct function_profile_data *, struct ssa_data_node *);

struct u8_set get_used_locals(struct ssa_data_node *);

//OP_GET_LOCAL
struct ssa_data_get_local_node {
    struct ssa_data_node data;

    profile_data_type_t type;

    int local_number;
    struct u8_set phi_versions;
};

//OP_CALL
struct ssa_control_function_call_node {
    struct ssa_data_node data;

    struct ssa_data_node * function;
    int n_arguments;
    bool is_parallel;
    struct ssa_data_node ** arguments;
};

//OP_GET_GLOBAL
struct ssa_data_get_global_node {
    struct ssa_data_node data;

    struct package * package;
    struct string_object * name;
};

//OP_EQUAL, OP_LESS, OP_GREATER
struct ssa_data_comparation_node {
    struct ssa_data_node data;

    struct ssa_data_node * left;
    profile_data_type_t left_type;

    struct ssa_data_node * right;
    profile_data_type_t right_type;

    bytecode_t operand;
};

//OP_ADD, OP_SUB, OP_MUL, OP_DIV
struct ssa_data_arithmetic_node {
    struct ssa_data_node data;

    struct ssa_data_node * left;
    profile_data_type_t left_type;

    struct ssa_data_node * right;
    profile_data_type_t right_type;

    bytecode_t operand;
};

//OP_CONST, OP_FAST_CONST_8, OP_FAST_CONST_16, OP_CONST_1, OP_CONST_2
struct ssa_data_constant_node {
    struct ssa_data_node data;

    profile_data_type_t type;
    union {
        struct string_object * string;
        struct object * object;
        int64_t i64;
        double f64;
        bool boolean;
        void * nil;
    } as;
};

//OP_NEGATE, OP_NOT
struct ssa_data_unary_node {
    struct ssa_data_node data;

    struct ssa_data_node * unary_value_node;
    enum {
        UNARY_OPERATION_TYPE_NOT,
        UNARY_OPERATION_TYPE_NEGATION,
    } unary_operation_type;
};

struct ssa_data_get_struct_field_node {
    struct ssa_data_node data;

    struct string_object * field_name;
    struct ssa_data_node * instance_node;
};

struct ssa_data_initialize_struct_node {
    struct ssa_data_node data;

    struct struct_definition_object * definition;
    struct ssa_data_node ** fields_nodes;
};

struct ssa_data_get_array_element_node {
    struct ssa_data_node data;

    int index;
    struct ssa_data_node * instance;
};

struct ssa_data_initialize_array_node {
    struct ssa_data_node data;

    int n_elements;
    bool empty_initialization;
    struct ssa_data_node ** elememnts_node;
};
