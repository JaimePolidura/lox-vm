#include "escape_analysis.h"

struct ea {
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator ea_allocator;

    //Key: ssa_name as u16,
    //Value: poiner to ssa_data_initialize_struct_node or ssa_data_initialize_array_node
    struct u64_hash_table instance_by_ssa_name;
    //Key: poiner to ssa_data_initialize_struct_node or ssa_data_initialize_array_node,
    //value: struct u64_set * with values of pointers to ssa_control_node
    struct u64_hash_table control_uses_by_instance;
    //Same as control_uses_by_instance but instead of pointer to control nodes, to data nodes
    struct u64_hash_table data_uses_by_instance;
};

struct ea * alloc_escape_analysis(struct ssa_ir *);
void free_escape_analysis(struct ea *);

static bool perform_escape_analysis_block(struct ssa_block*, void*);
static void perform_escape_analysis_control(struct ea*, struct ssa_control_node*);
static void perform_escape_analysis_define_ssa_name(struct ea*, struct ssa_control_define_ssa_name_node*);
static void mark_controol_node_input_nodes_as_escaped(struct ea *ea, struct ssa_control_node *control_node);
static bool control_node_escapes_inputs(struct ssa_control_node *);
static bool mark_control_node_input_nodes_as_escaped_consumer(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static void mark_data_node_as_escaped(struct ea*, struct ssa_data_node*);

void perform_escape_analysis(struct ssa_ir * ssa_ir) {
    struct ea * escape_analysis = alloc_escape_analysis(ssa_ir);

    for_each_ssa_block(
            ssa_ir->first_block,
            &escape_analysis->ea_allocator.lox_allocator,
            &escape_analysis,
            SSA_BLOCK_OPT_NOT_REPEATED,
            &perform_escape_analysis_block
    );

    free_escape_analysis(escape_analysis);
}

static bool perform_escape_analysis_block(struct ssa_block * block, void * extra) {
    struct ea * ea = extra;

    for (struct ssa_control_node * current_control = block->first;; current_control = current_control->next) {
        perform_escape_analysis_control(ea, current_control);

        if (current_control == block->last) {
            break;
        }
    }

    return true;
}

static void perform_escape_analysis_control(struct ea * ea, struct ssa_control_node * control_node) {
    if (control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
        perform_escape_analysis_define_ssa_name(ea, (struct ssa_control_define_ssa_name_node *) control_node);
    }
    if (control_node_escapes_inputs(control_node)) {
        mark_controol_node_input_nodes_as_escaped(ea, control_node);
    }
}

static void mark_controol_node_input_nodes_as_escaped(struct ea * ea, struct ssa_control_node * control_node) {
    for_each_data_node_in_control_node(control_node, ea, SSA_DATA_NODE_OPT_ANY_ORDER, mark_control_node_input_nodes_as_escaped_consumer);
}

static bool mark_control_node_input_nodes_as_escaped_consumer(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * child,
        void * extra
) {
    struct ea * ea = extra;
    mark_data_node_as_escaped(ea, child);

    return true;
}

static void mark_data_node_as_escaped(struct ea * ea, struct ssa_data_node * data_node) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        case SSA_DATA_NODE_TYPE_PHI:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
        case SSA_DATA_NODE_TYPE_GUARD:
        case SSA_DATA_NODE_TYPE_CALL:
        case SSA_DATA_NODE_TYPE_BINARY:
        case SSA_DATA_NODE_TYPE_UNARY:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH:
    }
}

static void perform_escape_analysis_define_ssa_name(struct ea * ea, struct ssa_control_define_ssa_name_node * define) {
    if(define->value->type == SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY || define->value->type == SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT){
        put_u64_hash_table(&ea->instance_by_ssa_name, define->ssa_name.u16, define->value);
    } else if (define->value->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME) {
        struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node * ) define->value;
        void * instance = get_u64_hash_table(&ea->instance_by_ssa_name, get_ssa_name->ssa_name.u16);
        put_u64_hash_table(&ea->instance_by_ssa_name, define->ssa_name.u16, instance);
    }
}

struct ea * alloc_escape_analysis(struct ssa_ir * ssa_ir) {
    struct ea * escape_analysis = NATIVE_LOX_MALLOC(sizeof(struct ea));
    escape_analysis->ssa_ir = ssa_ir;

    struct arena arena;
    init_arena(&arena);
    escape_analysis->ea_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&escape_analysis->instance_by_ssa_name, &escape_analysis->ea_allocator.lox_allocator);
    init_u64_hash_table(&escape_analysis->control_uses_by_instance, &escape_analysis->ea_allocator.lox_allocator);

    return escape_analysis;
}

void free_escape_analysis(struct ea * ea) {
    free_arena(&ea->ea_allocator.arena);
    NATIVE_LOX_FREE(ea);
}

static bool control_node_escapes_inputs(struct ssa_control_node * control_node) {
    switch (control_node->type) {
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            return set_struct_field->escapes;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) control_node;
            return get_array_element->escapes;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
            return true;
        case SSA_CONTROL_NODE_TYPE_DATA:
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_GUARD:
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME:
            return false;
    }
}