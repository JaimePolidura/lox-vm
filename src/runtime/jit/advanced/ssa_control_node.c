#include "ssa_control_node.h"

void * allocate_ssa_block_node(
        ssa_control_node_type type,
        size_t struct_size_bytes,
        struct ssa_block * block,
        struct lox_allocator * allocator
) {
    struct ssa_control_node * ssa_control_node = LOX_MALLOC(allocator, struct_size_bytes);
    memset(ssa_control_node, 0, struct_size_bytes);
    ssa_control_node->block = block;
    ssa_control_node->type = type;

    if (ssa_control_node->type == SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT) {
        ((struct ssa_control_set_array_element_node *) ssa_control_node)->requires_range_check = true;
    }

    return ssa_control_node;
}

void for_each_data_node_in_control_node(
        struct ssa_control_node * control_node,
        void * extra,
        long options,
        ssa_data_node_consumer_t consumer
) {
    struct u64_set children = get_children_ssa_control_node(control_node);
    FOR_EACH_U64_SET_VALUE(children, child_parent_field_ptr_u64) {
        struct ssa_data_node ** child_parent_field_ptr = (struct ssa_data_node **) child_parent_field_ptr_u64;
        struct ssa_data_node * child = *child_parent_field_ptr;

        for_each_ssa_data_node(child, (void**) child_parent_field_ptr, extra, options, consumer);
    }
}

struct u64_set get_children_ssa_control_node(struct ssa_control_node * control_node) {
    struct u64_set children;
    init_u64_set(&children, NATIVE_LOX_ALLOCATOR());

    switch (control_node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_control_data_node * data_node = (struct ssa_control_data_node *) control_node;
            add_u64_set(&children, (uint64_t) &data_node->data);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) control_node;
            add_u64_set(&children, (uint64_t) &return_node->data);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) control_node;
            add_u64_set(&children, (uint64_t) &print_node->data);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_global->value_node);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_local->new_local_value);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_struct_field->instance);
            add_u64_set(&children, (uint64_t) &set_struct_field->new_field_value);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) control_node;
            add_u64_set(&children, (uint64_t) &set_array_element->new_element_value);
            add_u64_set(&children, (uint64_t) &set_array_element->array);
            add_u64_set(&children, (uint64_t) &set_array_element->index);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * cond_jump = (struct ssa_control_conditional_jump_node *) control_node;
            add_u64_set(&children, (uint64_t) &cond_jump->condition);
            break;
        }
        case SSA_CONTROL_NODE_GUARD: {
            struct ssa_control_guard_node * guard = (struct ssa_control_guard_node *) control_node;
            add_u64_set(&children, (uint64_t) &guard->guard.value);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME: {
            struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) control_node;
            add_u64_set(&children, (uint64_t) &define_ssa_name->value);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
            break;
    }

    return children;
}

static bool get_used_ssa_names_ssa_control_node_consumer(
        struct ssa_data_node * __,
        void ** _,
        struct ssa_data_node * current_data_node,
        void * extra
) {
    struct u64_set * used_ssa_names = extra;
    struct u64_set used_ssa_names_in_current_data_node = get_used_ssa_names_ssa_data_node(
            current_data_node, NATIVE_LOX_ALLOCATOR()
    );
    union_u64_set(used_ssa_names, used_ssa_names_in_current_data_node);
    free_u64_set(&used_ssa_names_in_current_data_node);

    //Don't keep scanning from this node, because we have already scanned al sub datanodes
    //in get_used_ssa_names_ssa_data_node()
    return false;
}

struct u64_set get_used_ssa_names_ssa_control_node(struct ssa_control_node * control_node, struct lox_allocator * allocator) {
    struct u64_set used_ssa_names;
    init_u64_set(&used_ssa_names, allocator);

    for_each_data_node_in_control_node(
            control_node,
            &used_ssa_names,
            SSA_DATA_NODE_OPT_PRE_ORDER,
            &get_used_ssa_names_ssa_control_node_consumer
    );

    return used_ssa_names;
}

void mark_as_escaped_ssa_control_node(struct ssa_control_node * node) {
    switch (node->type) {
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) node;
            set_struct_field->escapes = true;
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) node;
            set_array_element->escapes = true;
            break;
        }
    }
}