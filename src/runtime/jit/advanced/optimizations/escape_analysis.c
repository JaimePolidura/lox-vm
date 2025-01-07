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
static void maybe_save_instance_define_ssa(struct ea*, struct ssa_control_define_ssa_name_node*);
static void mark_control_node_input_nodes_as_escaped(struct ea *ea, struct ssa_control_node *control_node);
static bool control_node_escapes_inputs(struct ssa_control_node *);
static bool mark_control_node_input_nodes_as_escaped_consumer(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static void mark_data_node_as_escaped(struct ea*, struct ssa_data_node*);
static struct ssa_data_node * get_instance_data_node(struct ssa_data_node *);
static bool perform_escape_analysis_data(struct ea*, struct ssa_control_node*, struct ssa_data_node*);
static bool does_ssa_name_escapes(struct ea*, struct ssa_name);
static bool is_control_set_operation_too_complex(struct ssa_control_node *);
static bool is_ssa_data_node_type_too_complex_to_set_instance(struct ssa_data_node *);

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
        maybe_save_instance_define_ssa(ea, (struct ssa_control_define_ssa_name_node *) control_node);
    }

    if (control_node_escapes_inputs(control_node) || is_control_set_operation_too_complex(control_node)) {
        mark_control_node_input_nodes_as_escaped(ea, control_node);
        return;
    }

    struct u64_set children = get_children_ssa_control_node(control_node);
    bool escapes = false;
    FOR_EACH_U64_SET_VALUE(children, child_parent_field_ptr_u64) {
        struct ssa_data_node * child = * ((struct ssa_data_node **) child_parent_field_ptr_u64);
        escapes |= perform_escape_analysis_data(ea, control_node, child);
    }

    if (escapes) {
        mark_as_escaped_ssa_control_node(control_node);
    }
}

//TODO Save control node usage
static bool perform_escape_analysis_data(
        struct ea * ea,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node
) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call_node = (struct ssa_data_function_call_node *) data_node;
            for (int i = 0; i < call_node->n_arguments; i++) {
                mark_data_node_as_escaped(ea, call_node->arguments[i]);
            }
            return true;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
            return true;
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
            return false;
        case SSA_DATA_NODE_TYPE_UNARY:
        case SSA_DATA_NODE_TYPE_GUARD:
        case SSA_DATA_NODE_TYPE_UNBOX:
        case SSA_DATA_NODE_TYPE_BOX:
        case SSA_DATA_NODE_TYPE_BINARY: {
            bool escapes = false;

            FOR_EACH_U64_SET_VALUE(get_children_ssa_data_node(data_node, &ea->ea_allocator.lox_allocator), child_ptr_u64) {
                struct ssa_data_node * child = (struct ssa_data_node *) child_ptr_u64;
                escapes |= perform_escape_analysis_data(ea, control_node, child);
            }

            return escapes;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) data_node;
            get_struct_field->escapes = perform_escape_analysis_data(ea, control_node, get_struct_field->instance_node);
            return get_struct_field->escapes;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_arr_element = (struct ssa_data_get_array_element_node *) data_node;
            get_arr_element->escapes = perform_escape_analysis_data(ea, control_node, get_arr_element->instance_node);
            return get_arr_element->escapes;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) data_node;
            bool escapes = false;
            for(int i = 0; i < init_struct->definition->n_fields; i++){
                struct ssa_data_node * struct_field = init_struct->fields_nodes[i];
                if(is_ssa_data_node_type_too_complex_to_set_instance(struct_field)){
                    escapes = true;
                } else {
                    escapes |= perform_escape_analysis_data(ea, control_node, struct_field);
                }
            }
            init_struct->escapes = escapes;
            return escapes;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) data_node;
            bool escapes = false;
            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                struct ssa_data_node * array_element = init_array->elememnts_node[i];

                if (is_ssa_data_node_type_too_complex_to_set_instance(array_element)) {
                    escapes = true;
                } else {
                    escapes |= perform_escape_analysis_data(ea, control_node, array_element);
                }
            }
            init_array->escapes = escapes;
            return escapes;
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) data_node;
            bool escapes = false;

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, ssa_version_in_phi) {
                escapes |= does_ssa_name_escapes(ea, ssa_version_in_phi);
            }

            return escapes;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get = (struct ssa_data_get_ssa_name_node *) data_node;
            return does_ssa_name_escapes(ea, get->ssa_name);
        }
    }
}

static void mark_control_node_input_nodes_as_escaped(struct ea * ea, struct ssa_control_node * control_node) {
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
    mark_as_escaped_ssa_data_node(data_node);
}

static void maybe_save_instance_define_ssa(struct ea * ea, struct ssa_control_define_ssa_name_node * define) {
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

static struct ssa_data_node * get_instance_data_node(struct ssa_data_node * data_node) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) data_node;
            return get_struct_field->instance_node;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_arr_ele = (struct ssa_data_get_array_element_node *) data_node;
            return get_arr_ele->instance_node;
        }
        default: {
            return NULL;
        }
    }
}

static bool does_ssa_name_escapes(struct ea * ea, struct ssa_name name) {
    bool escapes = false;

    if (contains_u64_hash_table(&ea->instance_by_ssa_name, name.u16)) {
        struct ssa_data_node * instance = (struct ssa_data_node *) get_u64_hash_table(
                &ea->instance_by_ssa_name, name.u16);
        escapes = is_escaped_ssa_data_node(instance);
    }

    return escapes;
}

//Set operatinos includes: set_struct_field, set_struct_instance
static bool is_control_set_operation_too_complex(struct ssa_control_node * control_node) {
    switch (control_node->type) {
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            return is_ssa_data_node_type_too_complex_to_set_instance(set_struct_field->new_field_value);
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_arr_element = (struct ssa_control_set_array_element_node *) control_node;
            return is_ssa_data_node_type_too_complex_to_set_instance(set_arr_element->new_element_value);
        }
        default:
            return false;
    }
}

static bool is_ssa_data_node_type_too_complex_to_set_instance(struct ssa_data_node * data_node) {
    return IS_ARRAY_SSA_TYPE(data_node->produced_type->type) || IS_STRUCT_SSA_TYPE(data_node->produced_type->type);
}