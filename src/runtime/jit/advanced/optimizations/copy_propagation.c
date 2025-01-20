#include "copy_propagation.h"

struct cp {
    struct lox_ir * lox_ir;
    struct stack_list pending;
};

static void replace_redudant_copy(struct lox_ir *, struct lox_ir_control_define_ssa_name_node *, struct lox_ir_control_node *);
static bool replace_redudant_copy_data_node(struct lox_ir_data_node *, void **, struct lox_ir_data_node *, void *);
static struct cp * alloc_copy_propagation(struct lox_ir *);
static void free_copy_propagation(struct cp *);
static void initialization(struct cp *);
static void propagation(struct cp *);
static bool can_be_replaced(struct lox_ir_control_define_ssa_name_node *, struct lox_ir_control_node *);
static void remove_unused_ssa_name_definition(struct cp *cp, struct lox_ir_control_define_ssa_name_node *definition_to_remove);

void perform_copy_propagation(struct lox_ir * ssa_ir) {
    struct cp * copy_propagation = alloc_copy_propagation(ssa_ir);

    initialization(copy_propagation);
    propagation(copy_propagation);

    free_copy_propagation(copy_propagation);
}

static void propagation(struct cp * cp) {
    while(!is_empty_stack_list(&cp->pending)) {
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64(pop_stack_list(&cp->pending));
        struct lox_ir_control_define_ssa_name_node * current_ssa_name_definition = get_u64_hash_table(
                &cp->lox_ir->definitions_by_ssa_name, current_ssa_name.u16);
        struct u64_set * control_nodes_that_uses_ssa_name = get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, current_ssa_name.u16);

        if (control_nodes_that_uses_ssa_name == NULL || size_u64_set((*control_nodes_that_uses_ssa_name)) == 0) {
            remove_unused_ssa_name_definition(cp, current_ssa_name_definition);

        } else if (size_u64_set((*control_nodes_that_uses_ssa_name)) == 1) {
            uint64_t control_node_that_uses_ssa_name_u64 = get_first_value_u64_set((*control_nodes_that_uses_ssa_name));
            struct lox_ir_control_node * control_node_that_uses_ssa_name = (struct lox_ir_control_node *) control_node_that_uses_ssa_name_u64;

            if(can_be_replaced(current_ssa_name_definition, control_node_that_uses_ssa_name)){
                replace_redudant_copy(cp->lox_ir, current_ssa_name_definition, control_node_that_uses_ssa_name);

                if(control_node_that_uses_ssa_name->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME){
                    struct lox_ir_control_define_ssa_name_node * d = (struct lox_ir_control_define_ssa_name_node *) control_node_that_uses_ssa_name;
                    push_stack_list(&cp->pending, (void *) GET_DEFINED_SSA_NAME(control_node_that_uses_ssa_name).u16);
                }
            }
        }
    }
}

static void initialization(struct cp * cp) {
    struct u64_hash_table_iterator ssa_names_iterator;
    init_u64_hash_table_iterator(&ssa_names_iterator, cp->lox_ir->definitions_by_ssa_name);

    while (has_next_u64_hash_table_iterator(ssa_names_iterator)) {
        struct u64_hash_table_entry ssa_name_entry = next_u64_hash_table_iterator(&ssa_names_iterator);
        struct lox_ir_control_define_ssa_name_node *ssa_name_definition = ssa_name_entry.value;
        struct ssa_name ssa_name = CREATE_SSA_NAME_FROM_U64(ssa_name_entry.key);
        struct u64_set * ssa_name_uses = get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, ssa_name.u16);

        if (ssa_name_uses == NULL || size_u64_set((*ssa_name_uses)) <= 1) {
            push_stack_list(&cp->pending, (void *) ssa_name.u16);
        }
    }
}

static void remove_unused_ssa_name_definition(struct cp * cp, struct lox_ir_control_define_ssa_name_node * definition_to_remove) {
    remove_u64_hash_table(&cp->lox_ir->definitions_by_ssa_name, definition_to_remove->ssa_name.u16);
    remove_control_node_lox_ir_block(definition_to_remove->control.block, &definition_to_remove->control);

    struct u64_set used_ssa_names_in_definition = get_used_ssa_names_lox_ir_data_node(definition_to_remove->value,
                                                                                      NATIVE_LOX_ALLOCATOR());
    FOR_EACH_U64_SET_VALUE(used_ssa_names_in_definition, used_ssa_name_in_definition_u64) {
        struct ssa_name name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_in_definition_u64);

        remove_u64_set(
                get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, used_ssa_name_in_definition_u64),
                (uint64_t) &definition_to_remove->control
        );
    }
}

static bool can_be_replaced(
        struct lox_ir_control_define_ssa_name_node * definition,
        struct lox_ir_control_node * use
) {
    bool belongs_to_code_motion = definition->control.block->nested_loop_body < use->block->nested_loop_body;
    bool definitions_is_phi = definition->value->type == LOX_IR_DATA_NODE_PHI;
    bool use_defines_phi = use->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME &&
                           ((struct lox_ir_control_define_ssa_name_node *) use)->value->type == LOX_IR_DATA_NODE_PHI;
    return !belongs_to_code_motion && !definitions_is_phi && !use_defines_phi;
}

struct replace_redudant_copy_struct {
    struct lox_ir_data_node * redundant_copy_value;
    struct ssa_name redundant_ssa_name;
    struct lox_ir * lox_ir;
};

static void replace_redudant_copy(
        struct lox_ir * lox_ir,
        struct lox_ir_control_define_ssa_name_node * copy_src_control,
        struct lox_ir_control_node * copy_dst
) {
    struct replace_redudant_copy_struct replace_redudant_copy_struct = (struct replace_redudant_copy_struct) {
            .redundant_copy_value = copy_src_control->value,
            .redundant_ssa_name = copy_src_control->ssa_name,
            .lox_ir = lox_ir,
    };

    for_each_data_node_in_lox_ir_control(
            copy_dst,
            &replace_redudant_copy_struct,
            LOX_IR_DATA_NODE_OPT_PRE_ORDER | LOX_IR_DATA_NODE_OPT_ONLY_TERMINATORS,
            replace_redudant_copy_data_node
    );

    remove_u64_hash_table(&lox_ir->definitions_by_ssa_name, copy_src_control->ssa_name.u16);
    remove_u64_hash_table(&lox_ir->node_uses_by_ssa_name, copy_src_control->ssa_name.u16);

    remove_control_node_lox_ir_block(copy_src_control->control.block, &copy_src_control->control);

    struct u64_set copy_src_control_used_ssa_names = get_used_ssa_names_lox_ir_data_node(
            copy_src_control->value, NATIVE_LOX_ALLOCATOR()
    );
    FOR_EACH_U64_SET_VALUE(copy_src_control_used_ssa_names, copy_src_control_used_ssa_name_u64) {
        struct ssa_name copy_src_control_used_ssa_name = CREATE_SSA_NAME_FROM_U64(copy_src_control_used_ssa_name_u64);
        add_ssa_name_use_lox_ir(lox_ir, copy_src_control_used_ssa_name, copy_dst);
    }
}

static bool replace_redudant_copy_data_node(
        struct lox_ir_data_node * _,
        void ** parent_child_ptr,
        struct lox_ir_data_node * current_node,
        void * extra
) {
    struct replace_redudant_copy_struct * replace_redudant_copy_struct = extra;
    struct lox_ir_data_node * redundant_copy_value = replace_redudant_copy_struct->redundant_copy_value;
    struct ssa_name redundant_ssa_name = replace_redudant_copy_struct->redundant_ssa_name;

    if (current_node->type == LOX_IR_DATA_NODE_GET_SSA_NAME && GET_USED_SSA_NAME(current_node).u16 == redundant_ssa_name.u16){
        *parent_child_ptr = replace_redudant_copy_struct->redundant_copy_value;
    }

    return true;
}

static struct cp * alloc_copy_propagation(struct lox_ir * lox_ir) {
    struct cp * cp =  NATIVE_LOX_MALLOC(sizeof(struct lox_ir));
    cp->lox_ir = lox_ir;
    init_stack_list(&cp->pending, NATIVE_LOX_ALLOCATOR());
    return cp;
}

static void free_copy_propagation(struct cp * cp) {
    free_stack_list(&cp->pending);
    free(cp);
}