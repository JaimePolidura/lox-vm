#include "copy_propagation.h"

struct cp {
    struct ssa_ir * ssa_ir;
    struct stack_list pending;
};

static void replace_redudant_copy(struct ssa_ir *, struct ssa_control_define_ssa_name_node *, struct ssa_control_node *);
static bool replace_redudant_copy_data_node(struct ssa_data_node *, void **, struct ssa_data_node *, void *);
static struct cp * alloc_copy_propagation(struct ssa_ir *);
static void free_copy_propagation(struct cp *);
static void initialization(struct cp *);
static void propagation(struct cp *);

void perform_copy_propagation(struct ssa_ir * ssa_ir) {
    struct cp * copy_propagation = alloc_copy_propagation(ssa_ir);

    initialization(copy_propagation);
    propagation(copy_propagation);

    free_copy_propagation(copy_propagation);
}

static void propagation(struct cp * cp) {
    while(!is_empty_stack_list(&cp->pending)) {
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64(pop_stack_list(&cp->pending));
        struct ssa_control_define_ssa_name_node * current_ssa_name_definition = get_u64_hash_table(
                &cp->ssa_ir->ssa_definitions_by_ssa_name, current_ssa_name.u16);

        struct u64_set * control_nodes_that_uses_ssa_name = get_u64_hash_table(&cp->ssa_ir->node_uses_by_ssa_name, current_ssa_name.u16);
        if (size_u64_set((*control_nodes_that_uses_ssa_name)) == 1) {
            uint64_t control_node_that_uses_ssa_name_u64 = get_first_value_u64_set((*control_nodes_that_uses_ssa_name));
            struct ssa_control_node * control_node_that_uses_ssa_name = (struct ssa_control_node *) control_node_that_uses_ssa_name_u64;

            replace_redudant_copy(cp->ssa_ir, current_ssa_name_definition, control_node_that_uses_ssa_name);
            
            if(control_node_that_uses_ssa_name->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME){
                push_stack_list(&cp->pending, (void *) GET_DEFINED_SSA_NAME(control_node_that_uses_ssa_name).u16);
            }
        }
    }
}

static void initialization(struct cp * cp) {
    struct u64_hash_table_iterator ssa_names_iterator;
    init_u64_hash_table_iterator(&ssa_names_iterator, cp->ssa_ir->ssa_definitions_by_ssa_name);

    while (has_next_u64_hash_table_iterator(ssa_names_iterator)) {
        struct u64_hash_table_entry ssa_name_entry = next_u64_hash_table_iterator(&ssa_names_iterator);
        struct ssa_control_define_ssa_name_node *ssa_name_definition = ssa_name_entry.value;
        struct ssa_name ssa_name = CREATE_SSA_NAME_FROM_U64(ssa_name_entry.key);

        push_stack_list(&cp->pending, (void *) ssa_name.u16);
    }
}

struct replace_redudant_copy_struct {
    struct ssa_data_node * redundant_copy_value;
    struct ssa_name redundant_ssa_name;
    struct ssa_ir * ssa_ir;
};

static void replace_redudant_copy(
        struct ssa_ir * ssa_ir,
        struct ssa_control_define_ssa_name_node * copy_src_control,
        struct ssa_control_node * copy_dst
) {
    struct replace_redudant_copy_struct replace_redudant_copy_struct = (struct replace_redudant_copy_struct) {
            .redundant_copy_value = copy_src_control->value,
            .redundant_ssa_name = copy_src_control->ssa_name,
            .ssa_ir = ssa_ir,
    };

    for_each_data_node_in_control_node(
            copy_dst,
            &replace_redudant_copy_struct,
            SSA_DATA_NODE_OPT_PRE_ORDER | SSA_DATA_NODE_OPT_ONLY_TERMINATORS,
            replace_redudant_copy_data_node
    );

    remove_u64_hash_table(&ssa_ir->ssa_definitions_by_ssa_name, copy_src_control->ssa_name.u16);
    remove_u64_hash_table(&ssa_ir->node_uses_by_ssa_name, copy_src_control->ssa_name.u16);

    struct u64_set copy_src_control_used_ssa_names = get_used_ssa_names_ssa_data_node(
            copy_src_control->value, NATIVE_LOX_ALLOCATOR()
    );
    FOR_EACH_U64_SET_VALUE(copy_src_control_used_ssa_names, copy_src_control_used_ssa_name_u64) {
        struct ssa_name copy_src_control_used_ssa_name = CREATE_SSA_NAME_FROM_U64(copy_src_control_used_ssa_name_u64);
        add_ssa_name_use_ssa_ir(ssa_ir, copy_src_control_used_ssa_name, copy_dst);
    }
}

static bool replace_redudant_copy_data_node(
        struct ssa_data_node * _,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct replace_redudant_copy_struct * replace_redudant_copy_struct = extra;
    struct ssa_data_node * redundant_copy_value = replace_redudant_copy_struct->redundant_copy_value;
    struct ssa_name redundant_ssa_name = replace_redudant_copy_struct->redundant_ssa_name;

    if (current_node->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME && GET_USED_SSA_NAME(current_node).u16 == redundant_ssa_name.u16){
        *parent_child_ptr = replace_redudant_copy_struct->redundant_copy_value;
    }

    return true;
}

static struct cp * alloc_copy_propagation(struct ssa_ir * ssa_ir) {
    struct cp * cp =  NATIVE_LOX_MALLOC(sizeof(struct ssa_ir));
    cp->ssa_ir = ssa_ir;
    init_stack_list(&cp->pending, NATIVE_LOX_ALLOCATOR());
    return cp;
}

static void free_copy_propagation(struct cp * cp) {
    free_stack_list(&cp->pending);
    free(cp);
}