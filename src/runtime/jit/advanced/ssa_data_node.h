#pragma once

#include "compiler/bytecode/bytecode_list.h"

#include "shared/utils/collections/u64_set.h"
#include "shared/utils/collections/u8_arraylist.h"
#include "shared/types/native_function_object.h"
#include "shared/types/struct_definition_object.h"
#include "shared/utils/collections/u8_set.h"
#include "shared/bytecode/bytecode.h"
#include "shared/package.h"
#include "shared.h"

#include "runtime/jit/advanced/ssa_guard.h"
#include "runtime/profiler/profile_data.h"

//Data flow nodes used in SSA IR
typedef enum {
    SSA_DATA_NODE_TYPE_CALL,
    SSA_DATA_NODE_TYPE_GET_LOCAL,
    SSA_DATA_NODE_TYPE_GET_GLOBAL,
    SSA_DATA_NODE_TYPE_BINARY,
    SSA_DATA_NODE_TYPE_CONSTANT,
    SSA_DATA_NODE_TYPE_UNARY,
    SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD,
    SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT,
    SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT,
    SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY,
    SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH,
    SSA_DATA_NODE_TYPE_GUARD,
    //Only used when inserting phi functions in the graph ir creation process.
    //It will replace all the nodes with type SSA_DATA_NODE_TYPE_GET_LOCAL in the phi insertion proceess
    SSA_DATA_NODE_TYPE_PHI,
    SSA_DATA_NODE_TYPE_GET_SSA_NAME,

    SSA_DATA_NODE_UNBOX,
    SSA_DATA_NODE_BOX,
} ssa_data_node_type;

#define ALLOC_SSA_DATA_NODE(type, struct_type, bytecode, allocator) (struct_type *) allocate_ssa_data_node(type, sizeof(struct_type), bytecode, allocator)
#define GET_CONST_VALUE_SSA_NODE(node) (((struct ssa_data_constant_node *) (node))->constant_lox_value)
#define GET_USED_SSA_NAME(node) (((struct ssa_data_get_ssa_name_node *) (node))->ssa_name)

//Represents expressions in CFG
struct ssa_data_node {
    struct bytecode_list * original_bytecode;
    profile_data_type_t produced_type;
    ssa_data_node_type type;
};

//Start control_node is inclusive. The iteration order will be post order
void * allocate_ssa_data_node(ssa_data_node_type type, size_t struct_size_bytes, struct bytecode_list *, struct lox_allocator *);
void free_ssa_data_node(struct ssa_data_node *);

enum {
    SSA_DATA_NODE_OPT_ANY_ORDER = 1 << 0,
    SSA_DATA_NODE_OPT_POST_ORDER = 1 << 0,
    SSA_DATA_NODE_OPT_PRE_ORDER = 1 << 3,
    SSA_DATA_NODE_OPT_NOT_TERMINATORS = 1 << 4, //AKA Don't scan leafs. Disabled by default
    SSA_DATA_NODE_OPT_ONLY_TERMINATORS = 1 << 5, //AKA scan leafs. Disabled by default
};
typedef bool (*ssa_data_node_consumer_t)(
        struct ssa_data_node * parent,
        void ** parent_child_ptr,
        struct ssa_data_node * child,
        void * extra
);
//Iterates all nodes. If the consumer return false, the node's children won't be scanned.
void for_each_ssa_data_node(struct ssa_data_node *, void **, void *, long options, ssa_data_node_consumer_t);

struct ssa_data_constant_node * create_ssa_const_node(lox_value_t, struct bytecode_list *, struct lox_allocator *);
struct u8_set get_used_locals_ssa_data_node(struct ssa_data_node *);
//Returns hashcode for ssa_data_node. The hashcode calculation should be the same for commative & associative data nodes.
//Example: Hash(a + b) == Hash(b + a) Or Hash((a + b) + c) == Hash(b + (a + c))
uint64_t hash_ssa_data_node(struct ssa_data_node *);
//Returns true if the two data nodes are equal. The calculation takes into account if nodes are commative & associative.
//Example: (a + b) == (b + a) Or ((a + b) + c) == (b + (a + c))
bool is_eq_ssa_data_node(struct ssa_data_node *, struct ssa_data_node *, struct lox_allocator *);
struct u64_set get_used_ssa_names_ssa_data_node(struct ssa_data_node *, struct lox_allocator *);
//A terminator node is a node that has no children
bool is_terminator_ssa_data_node(struct ssa_data_node *);

//OP_GET_LOCAL
struct ssa_data_get_local_node {
    struct ssa_data_node data;
    int local_number;
};

//OP_CALL
struct ssa_data_function_call_node {
    struct ssa_data_node data;

    int n_arguments;
    struct ssa_data_node ** arguments;

    bool is_native;

    union {
        struct native_function_object * native_function;

        struct {
            struct function_object * function;
            bool is_parallel;
        } lox_function;
    };

};

//OP_GET_GLOBAL
struct ssa_data_get_global_node {
    struct ssa_data_node data;

    struct package * package;
    struct string_object * name;
};

//OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_EQUAL, OP_LESS, OP_GREATER
struct ssa_data_binary_node {
    struct ssa_data_node data;

    bytecode_t operand;
    struct ssa_data_node * left;
    struct ssa_data_node * right;
};

struct ssa_data_get_array_length {
    struct ssa_data_node data;
    struct ssa_data_node * instance;
};

//OP_CONST, OP_FAST_CONST_8, OP_FAST_CONST_16, OP_CONST_1, OP_CONST_2
struct ssa_data_constant_node {
    struct ssa_data_node data;

    lox_value_t constant_lox_value;

    //These fields won't be heap allocated when creating a ssa_data_constant_node
    union {
        struct string_object * string;
        struct object * object;
        int64_t i64;
        double f64;
        bool boolean;
        void * nil;
    } value_as;
};

//TODO Replace it with bytecode_t in struct ssa_data_unary_node
typedef enum {
    UNARY_OPERATION_TYPE_NOT,
    UNARY_OPERATION_TYPE_NEGATION,
} ssa_unary_operator_type_t;

//OP_NEGATE, OP_NOT
struct ssa_data_unary_node {
    struct ssa_data_node data;

    struct ssa_data_node * operand;
    ssa_unary_operator_type_t operator_type;
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

    struct ssa_data_node * index;
    struct ssa_data_node * instance;
};

struct ssa_data_initialize_array_node {
    struct ssa_data_node data;

    int n_elements;
    bool empty_initialization;
    struct ssa_data_node ** elememnts_node;
};

//These nodes will be only used when inserting phi functions in the graph ir creation process
#define CREATE_SSA_NAME(local_number, version) (struct ssa_name) {.value = {local_number, version}}
#define CREATE_SSA_NAME_FROM_U64(u64_value) (struct ssa_name) {.u16 = (uint16_t) u64_value}

struct ssa_name {
    union {
        struct {
            uint8_t local_number;
            uint8_t version;
        } value;
        uint16_t u16;
    };
};

#define FOR_EACH_SSA_NAME_IN_PHI_NODE(phi_node, current_name) \
    struct u64_set_iterator iterator##current_name; \
    init_u64_set_iterator(&iterator##current_name, (phi_node)->ssa_versions); \
    struct ssa_name current_name = has_next_u64_set_iterator(iterator##current_name) ? \
        CREATE_SSA_NAME((phi_node)->local_number, next_u64_set_iterator(&iterator##current_name)) : \
        CREATE_SSA_NAME_FROM_U64(0); \
    for(int i##current_name = 0; \
        i##current_name < size_u64_set((phi_node)->ssa_versions); \
        i##current_name++, current_name = has_next_u64_set_iterator(iterator##current_name) ? \
            CREATE_SSA_NAME((phi_node)->local_number, next_u64_set_iterator(&iterator##current_name)) : \
            CREATE_SSA_NAME_FROM_U64(0))

struct ssa_data_phi_node {
    struct ssa_data_node data;
    uint8_t local_number;

    struct u64_set ssa_versions;
};

//Will replace OP_GET_LOCAL, when a variable
struct ssa_data_get_ssa_name_node {
    struct ssa_data_node data;
    struct ssa_name ssa_name;
};

struct ssa_data_guard_node {
    struct ssa_data_node data;
    struct ssa_guard guard;
};

struct ssa_data_node_box {
    struct ssa_data_node data;
    struct ssa_data_node * to_box;
};

struct ssa_data_node_unbox {
    struct ssa_data_node data;
    struct ssa_data_node * to_unbox;
};