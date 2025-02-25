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

#include "runtime/jit/advanced/phi_resolution/v_register.h"
#include "runtime/jit/advanced/lox_ir_guard.h"
#include "runtime/jit/advanced/lox_ir_type.h"
#include "runtime/profiler/profile_data.h"

typedef enum {
    LOX_IR_DATA_NODE_CALL,
    LOX_IR_DATA_NODE_GET_LOCAL,
    LOX_IR_DATA_NODE_GET_GLOBAL,
    LOX_IR_DATA_NODE_BINARY,
    LOX_IR_DATA_NODE_CONSTANT,
    LOX_IR_DATA_NODE_UNARY,
    LOX_IR_DATA_NODE_GET_STRUCT_FIELD,
    LOX_IR_DATA_NODE_INITIALIZE_STRUCT,
    LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT,
    LOX_IR_DATA_NODE_INITIALIZE_ARRAY,
    LOX_IR_DATA_NODE_GET_ARRAY_LENGTH,
    LOX_IR_DATA_NODE_GUARD,
    //Only used when inserting phi functions in the graph ir creation process.
    //It will replace all the nodes with type LOX_IR_DATA_NODE_GET_LOCAL in the phi insertion proceess
    LOX_IR_DATA_NODE_PHI,
    LOX_IR_DATA_NODE_GET_SSA_NAME,
    LOX_IR_DATA_NODE_CAST,
    //Intrudcued by phi resolution phase, this is not used in optimizations
    LOX_IR_DATA_NODE_GET_V_REGISTER,
} lox_ir_data_node_type;

#define ALLOC_LOX_IR_DATA(type, struct_type, bytecode, allocator) (struct_type *) allocate_lox_ir_data_node(type, sizeof(struct_type), bytecode, allocator)
#define GET_CONST_VALUE_LOX_IR_NODE(node) (((struct lox_ir_data_constant_node *) (node))->value)
#define GET_USED_SSA_NAME(node) (((struct lox_ir_data_get_ssa_name_node *) (node))->ssa_name)

//Represents expressions in CFG
struct lox_ir_data_node {
    struct bytecode_list * original_bytecode;
    //The type produced by this data control
    //This is added in type_propagation optimization process
    struct lox_ir_type * produced_type;
    lox_ir_data_node_type type;
};

//Start control_node_to_lower is inclusive. The iteration order will be post order
void * allocate_lox_ir_data_node(lox_ir_data_node_type type, size_t struct_size_bytes, struct bytecode_list *, struct lox_allocator *);
void free_lox_ir_data_node(struct lox_ir_data_node *node_to_free);

enum {
    LOX_IR_DATA_NODE_OPT_ANY_ORDER = 1 << 0,
    LOX_IR_DATA_NODE_OPT_POST_ORDER = 1 << 0,
    LOX_IR_DATA_NODE_OPT_PRE_ORDER = 1 << 3,
    LOX_IR_DATA_NODE_OPT_NOT_TERMINATORS = 1 << 4, //AKA Don't scan leafs. Disabled by default
    LOX_IR_DATA_NODE_OPT_ONLY_TERMINATORS = 1 << 5, //AKA scan leafs. Disabled by default
};
typedef bool (*lox_ir_data_node_consumer_t)(
        struct lox_ir_data_node * parent,
        void ** parent_child_ptr,
        struct lox_ir_data_node * child,
        void * extra
);
//Iterates all nodes. If the consumer return false, the control's children won't be scanned.
bool for_each_lox_ir_data_node(struct lox_ir_data_node*, void**, void*, long, lox_ir_data_node_consumer_t);

struct lox_ir_data_constant_node * create_lox_ir_const_node(uint64_t, lox_ir_type_t, struct bytecode_list*, struct lox_allocator*);
struct u8_set get_used_locals_lox_ir_data_node(struct lox_ir_data_node*);
//Returns hashcode for lox_ir_data_node. The hashcode calculation should be the same for commative & associative data nodes.
//Example: Hash(a + b) == Hash(b + a) Or Hash((a + b) + c) == Hash(b + (a + c))
uint64_t hash_lox_ir_data_node(struct lox_ir_data_node*);
//Returns true if the two data nodes are equal. The calculation takes into account if nodes are commative & associative.
//Example: (a + b) == (b + a) Or ((a + b) + c) == (b + (a + c))
bool is_eq_lox_ir_data_node(struct lox_ir_data_node*, struct lox_ir_data_node*, struct lox_allocator*);
struct u64_set get_used_ssa_names_lox_ir_data_node(struct lox_ir_data_node*, struct lox_allocator*);
struct u64_set get_used_v_registers_lox_ir_data_node(struct lox_ir_data_node*, struct lox_allocator*);
//A terminator control is a control that has no children
bool is_terminator_lox_ir_data_node(struct lox_ir_data_node*);
//Returns set of pointers to the fields of parent that contains the children pointer. Type: struct lox_ir_data_node **
struct u64_set get_children_lox_ir_data_node(struct lox_ir_data_node*, struct lox_allocator*);
void const_to_native_lox_ir_data_node(struct lox_ir_data_constant_node *const_node);
bool is_marked_as_escaped_lox_ir_data_node(struct lox_ir_data_node*);
//Sets escapes boolean field in certain type of lox ir nodes to true
void mark_as_escaped_lox_ir_data_node(struct lox_ir_data_node*);

//OP_GET_LOCAL
struct lox_ir_data_get_local_node {
    struct lox_ir_data_node data;
    int local_number;
    //Same size as get_v_register so that it can be replaced easily in the graph ir
    uint8_t padding[8];
};

//OP_CALL
struct lox_ir_data_function_call_node {
    struct lox_ir_data_node data;

    int n_arguments;
    struct lox_ir_data_node ** arguments;

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
struct lox_ir_data_get_global_node {
    struct lox_ir_data_node data;

    struct package * package;
    struct string_object * name;
};

//ADD SUB MUL DIV GREATER LESS EQUAL BINARY_OP_AND BINARY_OP_OR LEFT_SHIFT RIGHT_SHIFT MODULO
struct lox_ir_data_binary_node {
    struct lox_ir_data_node data;

    bytecode_t operator;
    struct lox_ir_data_node * left;
    struct lox_ir_data_node * right;
};

struct lox_ir_data_get_array_length {
    struct lox_ir_data_node data;
    struct lox_ir_data_node * instance;
    bool escapes;
};

//OP_CONST, OP_FAST_CONST_8, OP_FAST_CONST_16, OP_CONST_1, OP_CONST_2
struct lox_ir_data_constant_node {
    struct lox_ir_data_node data;
    uint64_t value;
};

//TODO Replace it with bytecode_t in struct lox_ir_data_unary_node
typedef enum {
    UNARY_OPERATION_TYPE_NOT,
    UNARY_OPERATION_TYPE_NEGATION,
} lox_ir_unary_operator_type_t;

//OP_NEGATE, OP_NOT
struct lox_ir_data_unary_node {
    struct lox_ir_data_node data;

    struct lox_ir_data_node * operand;
    lox_ir_unary_operator_type_t operator;
};

struct lox_ir_data_get_struct_field_node {
    struct lox_ir_data_node data;

    struct string_object * field_name;
    struct lox_ir_data_node * instance;
    bool escapes;
};

struct lox_ir_data_initialize_struct_node {
    struct lox_ir_data_node data;

    struct struct_definition_object * definition;
    struct lox_ir_data_node ** fields_nodes;
    bool escapes;
};

struct lox_ir_data_get_array_element_node {
    struct lox_ir_data_node data;

    struct lox_ir_data_node * index;
    struct lox_ir_data_node * instance;
    bool escapes;
    bool requires_range_check;
};

struct lox_ir_data_initialize_array_node {
    struct lox_ir_data_node data;

    int n_elements;
    bool empty_initialization;
    struct lox_ir_data_node ** elememnts;
    bool escapes;
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

struct lox_ir_data_phi_node {
    struct lox_ir_data_node data;
    uint8_t local_number;

    //Set of versions
    struct u64_set ssa_versions;
};

//Will replace OP_GET_LOCAL, when a variable
struct lox_ir_data_get_ssa_name_node {
    struct lox_ir_data_node data;
    struct ssa_name ssa_name;
    //Same size as get_v_register so that it can be replaced easily in the graph ir
    uint8_t padding[8];
};

struct lox_ir_data_guard_node {
    struct lox_ir_data_node data;
    struct lox_ir_guard guard;
};

struct lox_ir_data_guard_node * create_from_profile_lox_ir_data_guard_node(struct type_profile_data, struct lox_ir_data_node*,
        struct lox_allocator *, lox_ir_guard_action_on_check_failed);

struct lox_ir_data_cast_node {
    struct lox_ir_data_node data;
    struct lox_ir_data_node * to_cast;
};

struct lox_ir_data_get_v_register_node {
    struct lox_ir_data_node data;
    struct v_register v_register;
};

struct lox_ir_data_load_node_node {
    struct lox_ir_data_node data;
    struct lox_ir_data_node * pointer;
    uint16_t offset;
};