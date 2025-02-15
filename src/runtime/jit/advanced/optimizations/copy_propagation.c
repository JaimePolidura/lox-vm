#include "copy_propagation.h"

struct cp {
    struct lox_ir * lox_ir;
    struct stack_list pending;
    struct arena_lox_allocator cp_allocator;
};

struct pending_to_cp {
    union {
        int v_register_number;
        struct ssa_name ssa_name;
    };
    bool is_ssa_name;
};

static struct pending_to_cp * create_pending_ssa_name(struct cp*, struct ssa_name);
static struct pending_to_cp * create_pending_v_register(struct cp*, int v_register_number);
static void replace_redudant_copy(struct cp *, struct pending_to_cp *, struct lox_ir_control_node *);
static bool replace_redudant_copy_data_node(struct lox_ir_data_node *, void **, struct lox_ir_data_node *, void *);
static struct cp * alloc_copy_propagation(struct lox_ir *);
static void free_copy_propagation(struct cp *);
static void initialization(struct cp *);
static void propagation(struct cp *);
static bool can_be_replaced(struct cp*, struct pending_to_cp*, struct lox_ir_control_node*);
static void remove_unused_ssa_name_definition(struct cp *cp, struct lox_ir_control_define_ssa_name_node *);
static void remove_unused_v_register_definition(struct cp *cp, struct lox_ir_control_set_v_register_node *);
static void remove_unused_definition(struct cp * cp, struct pending_to_cp*, struct lox_ir_control_node *);
static void replace_redudant_copy_ssa_name(struct cp *,struct lox_ir_control_define_ssa_name_node*,struct lox_ir_control_node *);
static void replace_redudant_copy_v_reg(struct cp *,struct lox_ir_control_set_v_register_node*,struct lox_ir_control_node *);

static struct u64_set * get_definitions(struct cp*, struct pending_to_cp*);
static struct u64_set * get_uses(struct cp*, struct pending_to_cp*);
static struct lox_ir_data_node * get_definition_value(struct pending_to_cp*, struct lox_ir_control_node *);
static void push_pending_to_propagate(struct cp*, struct lox_ir_control_node *);

void perform_copy_propagation(struct lox_ir * lox_ir) {
    struct cp * copy_propagation = alloc_copy_propagation(lox_ir);

    initialization(copy_propagation);
    propagation(copy_propagation);

    free_copy_propagation(copy_propagation);
}

static void propagation(struct cp * cp) {
    while(!is_empty_stack_list(&cp->pending)) {
        struct pending_to_cp * pending_to_cp = pop_stack_list(&cp->pending);
        struct u64_set * control_nodes_that_uses_ssa_name = get_uses(cp, pending_to_cp);

        if (control_nodes_that_uses_ssa_name == NULL || size_u64_set((*control_nodes_that_uses_ssa_name)) == 0) {
            FOR_EACH_U64_SET_VALUE(*get_definitions(cp, pending_to_cp), control_definition_ptr_u64) {
                remove_unused_definition(cp, pending_to_cp, (struct lox_ir_control_node *) control_definition_ptr_u64);
            }

        } else if (size_u64_set((*control_nodes_that_uses_ssa_name)) == 1) {
            uint64_t control_node_that_uses_ssa_name_u64 = get_first_value_u64_set((*control_nodes_that_uses_ssa_name));
            struct lox_ir_control_node * control_node_that_uses_ssa_name = (struct lox_ir_control_node *) control_node_that_uses_ssa_name_u64;

            if(can_be_replaced(cp, pending_to_cp, control_node_that_uses_ssa_name)){
                replace_redudant_copy(cp, pending_to_cp, control_node_that_uses_ssa_name);
                push_pending_to_propagate(cp, control_node_that_uses_ssa_name);
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
            push_stack_list(&cp->pending, create_pending_ssa_name(cp, ssa_name));
        }
    }

    for(int i = 0; i < cp->lox_ir->last_v_reg_allocated; i++){
        struct u64_set * v_register_uses = get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, i);
        if (v_register_uses == NULL || size_u64_set((*v_register_uses)) <= 1) {
            push_stack_list(&cp->pending, create_pending_v_register(cp, i));
        }
    }
}

static void remove_unused_definition(struct cp * cp, struct pending_to_cp * pending_to_cp, struct lox_ir_control_node * control_node) {
    if (pending_to_cp->is_ssa_name) {
        remove_unused_ssa_name_definition(cp, (struct lox_ir_control_define_ssa_name_node *) control_node);
    } else {
        remove_unused_v_register_definition(cp, (struct lox_ir_control_set_v_register_node *) control_node);
    }
}

static void remove_unused_v_register_definition(struct cp *cp, struct lox_ir_control_set_v_register_node *definition_to_remove) {
    remove_u64_hash_table(&cp->lox_ir->definitions_by_v_reg, definition_to_remove->v_register.number);
    remove_control_node_lox_ir_block(definition_to_remove->control.block, &definition_to_remove->control);

    struct u64_set used_v_registers_in_definition = get_used_v_registers_lox_ir_data_node(definition_to_remove->value,
            &cp->cp_allocator.lox_allocator);

    FOR_EACH_U64_SET_VALUE(used_v_registers_in_definition, used_v_register_in_definition) {
        remove_u64_set(
                get_u64_hash_table(&cp->lox_ir->node_uses_by_v_reg, used_v_register_in_definition),
                (uint64_t) &definition_to_remove->control
        );
    }
}

static void remove_unused_ssa_name_definition(struct cp * cp, struct lox_ir_control_define_ssa_name_node * definition_to_remove) {
    remove_u64_hash_table(&cp->lox_ir->definitions_by_ssa_name, definition_to_remove->ssa_name.u16);
    remove_control_node_lox_ir_block(definition_to_remove->control.block, &definition_to_remove->control);

    struct u64_set used_ssa_names_in_definition = get_used_ssa_names_lox_ir_data_node(definition_to_remove->value,
            &cp->cp_allocator.lox_allocator);
    FOR_EACH_U64_SET_VALUE(used_ssa_names_in_definition, used_ssa_name_in_definition_u64) {
        struct ssa_name name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_in_definition_u64);

        remove_u64_set(
                get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, used_ssa_name_in_definition_u64),
                (uint64_t) &definition_to_remove->control
        );
    }
}

static bool can_be_replaced(
        struct cp * cp,
        struct pending_to_cp * pending_cp,
        struct lox_ir_control_node * use
) {
    bool can_be_replaced = true;

    FOR_EACH_U64_SET_VALUE(*get_definitions(cp, pending_cp), control_definition_ptr_u64) {
        struct lox_ir_control_node * definition = (struct lox_ir_control_node *) control_definition_ptr_u64;
        struct lox_ir_data_node * definition_value = get_definition_value(pending_cp, definition);

        bool belongs_to_code_motion = definition->block->nested_loop_body < use->block->nested_loop_body;
        bool definitions_is_phi = definition_value->type == LOX_IR_DATA_NODE_PHI;
        bool use_defines_phi = use->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME &&
                               ((struct lox_ir_control_define_ssa_name_node *) use)->value->type == LOX_IR_DATA_NODE_PHI;

        can_be_replaced &= !belongs_to_code_motion
                //We only check this for ssa names
                && (!pending_cp->is_ssa_name || !definitions_is_phi && !use_defines_phi);
    }

    return can_be_replaced;
}

static void replace_redudant_copy(
        struct cp * cp,
        struct pending_to_cp * pending_to_cp,
        struct lox_ir_control_node * copy_dst
) {
    struct lox_ir_control_node * definition = (struct lox_ir_control_node *) get_first_value_u64_set(*get_definitions(cp, pending_to_cp));
    struct lox_ir_data_node * redundant_copy_value = get_definition_value(pending_to_cp, definition);

    remove_control_node_lox_ir_block(definition->block, definition);

    if (pending_to_cp->is_ssa_name) {
        replace_redudant_copy_ssa_name(cp, (struct lox_ir_control_define_ssa_name_node *) definition, copy_dst);
    } else {
        replace_redudant_copy_v_reg(cp, (struct lox_ir_control_set_v_register_node *) definition, copy_dst);
    }
}

struct replace_redudant_copy_struct_ssa_name {
    struct lox_ir_data_node * redundant_copy_value;
    struct ssa_name redundant_ssa_name;
    int redundant_vregister_number;
    struct cp * cp;
};

static void replace_redudant_copy_v_reg(
        struct cp * cp,
        struct lox_ir_control_set_v_register_node * copy_src_control,
        struct lox_ir_control_node * copy_dst
) {
    struct replace_redudant_copy_struct_ssa_name replace_redudant_copy_struct = (struct replace_redudant_copy_struct_ssa_name) {
            .redundant_vregister_number = copy_src_control->v_register.number,
            .redundant_copy_value = copy_src_control->value,
            .cp = cp,
    };

    for_each_data_node_in_lox_ir_control(
            copy_dst,
            &replace_redudant_copy_struct,
            LOX_IR_DATA_NODE_OPT_PRE_ORDER | LOX_IR_DATA_NODE_OPT_ONLY_TERMINATORS,
            replace_redudant_copy_data_node
    );

    remove_u64_hash_table(&cp->lox_ir->definitions_by_v_reg, copy_src_control->v_register.number);
    remove_u64_hash_table(&cp->lox_ir->node_uses_by_v_reg, copy_src_control->v_register.number);

    struct u64_set copy_src_control_used_v_regs = get_used_v_registers_lox_ir_data_node(
            copy_src_control->value, NATIVE_LOX_ALLOCATOR()
    );
    FOR_EACH_U64_SET_VALUE(copy_src_control_used_v_regs, used_v_reg) {
        add_v_register_use_lox_ir(cp->lox_ir, used_v_reg, copy_dst);
    }
}

static void replace_redudant_copy_ssa_name(
        struct cp * cp,
        struct lox_ir_control_define_ssa_name_node * copy_src_control,
        struct lox_ir_control_node * copy_dst
) {
    struct replace_redudant_copy_struct_ssa_name replace_redudant_copy_struct = (struct replace_redudant_copy_struct_ssa_name) {
            .redundant_ssa_name = copy_src_control->ssa_name,
            .redundant_copy_value = copy_src_control->value,
            .cp = cp,
    };

    for_each_data_node_in_lox_ir_control(
            copy_dst,
            &replace_redudant_copy_struct,
            LOX_IR_DATA_NODE_OPT_PRE_ORDER | LOX_IR_DATA_NODE_OPT_ONLY_TERMINATORS,
            replace_redudant_copy_data_node
    );

    remove_u64_hash_table(&cp->lox_ir->definitions_by_ssa_name, copy_src_control->ssa_name.u16);
    remove_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, copy_src_control->ssa_name.u16);

    remove_control_node_lox_ir_block(copy_src_control->control.block, &copy_src_control->control);

    struct u64_set copy_src_control_used_ssa_names = get_used_ssa_names_lox_ir_data_node(
            copy_src_control->value, NATIVE_LOX_ALLOCATOR()
    );
    FOR_EACH_U64_SET_VALUE(copy_src_control_used_ssa_names, copy_src_control_used_ssa_name_u64) {
        struct ssa_name copy_src_control_used_ssa_name = CREATE_SSA_NAME_FROM_U64(copy_src_control_used_ssa_name_u64);
        add_ssa_name_use_lox_ir(cp->lox_ir, copy_src_control_used_ssa_name, copy_dst);
    }
}

static bool replace_redudant_copy_data_node(
        struct lox_ir_data_node * _,
        void ** parent_child_ptr,
        struct lox_ir_data_node * current_node,
        void * extra
) {
    struct replace_redudant_copy_struct_ssa_name * replace_redudant_copy_struct = extra;
    struct lox_ir_data_node * redundant_copy_value = replace_redudant_copy_struct->redundant_copy_value;
    struct ssa_name redundant_ssa_name = replace_redudant_copy_struct->redundant_ssa_name;
    int redundant_vregister_number = replace_redudant_copy_struct->redundant_vregister_number;

    if (current_node->type == LOX_IR_DATA_NODE_GET_SSA_NAME && GET_USED_SSA_NAME(current_node).u16 == redundant_ssa_name.u16){
        *parent_child_ptr = replace_redudant_copy_struct->redundant_copy_value;
    } else if(current_node->type == LOX_IR_DATA_NODE_GET_V_REGISTER
        && ((struct lox_ir_data_get_v_register_node *) current_node)->v_register.number == redundant_vregister_number) {
        *parent_child_ptr = replace_redudant_copy_struct->redundant_copy_value;
    }

    return true;
}

static struct cp * alloc_copy_propagation(struct lox_ir * lox_ir) {
    struct cp * cp =  NATIVE_LOX_MALLOC(sizeof(struct lox_ir));
    cp->lox_ir = lox_ir;
    init_stack_list(&cp->pending, NATIVE_LOX_ALLOCATOR());
    struct arena arena;
    init_arena(&arena);
    cp->cp_allocator = to_lox_allocator_arena(arena);
    return cp;
}

static void free_copy_propagation(struct cp * cp) {
    free_stack_list(&cp->pending);
    free_arena(&cp->cp_allocator.arena);
    free(cp);
}

static struct pending_to_cp * create_pending_ssa_name(struct cp * cp, struct ssa_name ssa_name) {
    struct pending_to_cp * pending_to_cp = LOX_MALLOC(&cp->cp_allocator.lox_allocator, sizeof(struct pending_to_cp));
    pending_to_cp->ssa_name = ssa_name;
    pending_to_cp->is_ssa_name = true;
    return pending_to_cp;
}

static struct pending_to_cp * create_pending_v_register(struct cp * cp, int v_register_number) {
    struct pending_to_cp * pending_to_cp = LOX_MALLOC(&cp->cp_allocator.lox_allocator, sizeof(struct pending_to_cp));
    pending_to_cp->v_register_number = v_register_number;
    pending_to_cp->is_ssa_name = false;
    return pending_to_cp;
}

static struct u64_set * get_definitions(struct cp * cp, struct pending_to_cp * pending_to_cp) {
    if (pending_to_cp->is_ssa_name) {
        struct lox_ir_control_node * definition = get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, pending_to_cp->ssa_name.u16);
        struct u64_set * single_definition_set = LOX_MALLOC(&cp->cp_allocator.lox_allocator, sizeof(struct u64_set));
        init_u64_set(single_definition_set, &cp->cp_allocator.lox_allocator);
        add_u64_set(single_definition_set, (uint64_t) definition);

        return single_definition_set;
    } else {
        return get_u64_hash_table(&cp->lox_ir->node_uses_by_v_reg, pending_to_cp->v_register_number);
    }
}

static struct u64_set * get_uses(struct cp * cp, struct pending_to_cp * pending_to_cp) {
    if (pending_to_cp->is_ssa_name) {
        return get_u64_hash_table(&cp->lox_ir->node_uses_by_ssa_name, pending_to_cp->ssa_name.u16);
    } else {
        return get_u64_hash_table(&cp->lox_ir->node_uses_by_v_reg, pending_to_cp->v_register_number);
    }
}

static struct lox_ir_data_node * get_definition_value(struct pending_to_cp * pending_to_cp, struct lox_ir_control_node * control) {
    if (pending_to_cp->is_ssa_name) {
        return ((struct lox_ir_control_define_ssa_name_node *) control)->value;
    } else {
        return ((struct lox_ir_control_set_v_register_node *) control)->value;
    }
}

static void push_pending_to_propagate(struct cp * cp, struct lox_ir_control_node * definition) {
    if (definition->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        push_stack_list(&cp->pending, create_pending_ssa_name(cp, GET_DEFINED_SSA_NAME(definition)));
    } else if (definition->type == LOX_IR_CONTROL_NODE_SET_V_REGISTER) {
        struct lox_ir_control_set_v_register_node * set_v_reg = (struct lox_ir_control_set_v_register_node *) definition;
        push_stack_list(&cp->pending, create_pending_v_register(cp, set_v_reg->v_register.number));
    }
}