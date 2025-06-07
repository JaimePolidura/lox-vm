#include "phi_resolver.h"

struct pr {
    struct u64_hash_table v_register_by_local_number;

    struct arena_lox_allocator ps_allocator;
    struct lox_ir * lox_ir;
};

struct pr_control {
    struct pr * pr;
    struct lox_ir_control_node * current_control;
};

static bool resolve_phi_block(struct lox_ir_block *, void *);

static struct pr * alloc_phi_resolver(struct lox_ir *);
static void free_phi_resolver(struct pr *);
static void resolve_phi_control(struct pr *, struct lox_ir_control_node *);
static void replace_define_ssa_name_with_set_v_reg(struct pr*, struct lox_ir_control_define_ssa_name_node*);
static void replace_get_ssa_name_with_get_v_reg(struct pr_control*, struct lox_ir_data_get_ssa_name_node*);
static bool replace_phi_with_get_v_reg(struct pr*, struct lox_ir_data_phi_node*, struct lox_ir_control_define_ssa_name_node*);
static bool resolve_phi_data(struct lox_ir_data_node*, void**, struct lox_ir_data_node*, void*);
static bool all_predecessors_have_been_scanned(struct pr *, struct lox_ir_data_phi_node *);
static bool is_define_phi_node(struct lox_ir_control_node *);
static struct v_register get_v_reg_by_ssa_name(struct pr *, struct ssa_name, bool is_float);

void resolve_phi(struct lox_ir * lox_ir) {
//    struct pr * phi_resolver = alloc_phi_resolver(lox_ir);
//
//    for_each_lox_ir_block(
//            lox_ir->first_block,
//            NATIVE_LOX_ALLOCATOR(),
//            phi_resolver,
//            LOX_IR_BLOCK_OPT_REPEATED,
//            resolve_phi_block
//    );
//
//    free_phi_resolver(phi_resolver);
}

static bool resolve_phi_block(struct lox_ir_block * block, void * extra) {
    struct pr * pr = extra;

    for (struct lox_ir_control_node * current = block->first;; current = current->next) {
        resolve_phi_control(pr, current);

        if (current == block->last) {
            break;
        }
    }

    return true;
}

//This functino will return false, if we don't have enough information to keep scannning, for example when we dont know
//all mappings to v_reg given a phi control, because we haven't scanned all predecessors blocks
static void resolve_phi_control(struct pr * pr, struct lox_ir_control_node * control_node) {
    bool is_phi_node = is_define_phi_node(control_node);

    struct pr_control pr_control = (struct pr_control) {
        .current_control = control_node,
        .pr = pr,
    };

    for_each_data_node_in_lox_ir_control(
            control_node,
            &pr_control,
            LOX_IR_DATA_NODE_OPT_ANY_ORDER,
            resolve_phi_data
    );

    if (control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME && !is_phi_node) {
        replace_define_ssa_name_with_set_v_reg(pr, (struct lox_ir_control_define_ssa_name_node *) control_node);
    }
}

static bool resolve_phi_data(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * data_node,
        void * extra
) {
    struct pr_control * pr_control = extra;

    if (data_node->type == LOX_IR_DATA_NODE_PHI) {
        struct lox_ir_control_define_ssa_name_node * define_ssa_name = (struct lox_ir_control_define_ssa_name_node *) pr_control->current_control;
        remove_block_control_node_lox_ir(pr_control->pr->lox_ir, define_ssa_name->control.block, &define_ssa_name->control);
    } else if (data_node->type == LOX_IR_DATA_NODE_GET_SSA_NAME) {
        replace_get_ssa_name_with_get_v_reg(pr_control, (struct lox_ir_data_get_ssa_name_node *) data_node);
    }

    return true;
}

static void replace_get_ssa_name_with_get_v_reg(struct pr_control * pr_control, struct lox_ir_data_get_ssa_name_node * get_ssa_name) {
//    bool is_float = get_ssa_name->data.produced_type->type == LOX_IR_TYPE_F64;
//    struct v_register v_reg = get_v_reg_by_ssa_name(pr_control->pr, get_ssa_name->ssa_name, is_float);
//    get_ssa_name->data.type = LOX_IR_DATA_NODE_GET_V_REGISTER;
//    struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) get_ssa_name;
//    get_v_reg->v_register = v_reg;
}

static void replace_define_ssa_name_with_set_v_reg(struct pr * pr, struct lox_ir_control_define_ssa_name_node * define_ssa_name) {
//    bool is_float = define_ssa_name->value->produced_type->type == LOX_IR_TYPE_F64;
//    struct lox_ir_data_node * value = define_ssa_name->value;
//    struct v_register new_v_reg = get_v_reg_by_ssa_name(pr, define_ssa_name->ssa_name, is_float);
//
//    define_ssa_name->control.type = LOX_IR_CONTROL_NODE_SET_V_REGISTER;
//    struct lox_ir_control_set_v_register_node * set_v_reg = (struct lox_ir_control_set_v_register_node *) define_ssa_name;
//    set_v_reg->v_register = new_v_reg;
//    set_v_reg->value = value;
}

static bool is_define_phi_node(struct lox_ir_control_node * control_node) {
    return control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME &&
           ((struct lox_ir_control_define_ssa_name_node *) control_node)->value->type == LOX_IR_DATA_NODE_PHI;
}

static struct pr * alloc_phi_resolver(struct lox_ir * lox_ir) {
//    struct pr * pr = NATIVE_LOX_MALLOC(sizeof(struct pr));
//    struct arena arena;
//    init_arena(&arena);
//    pr->lox_ir = lox_ir;
//    pr->ps_allocator = to_lox_allocator_arena(arena);
//    init_u64_hash_table(&pr->v_register_by_local_number, &pr->ps_allocator.lox_allocator);
//
//    lox_ir->last_v_reg_allocated = lox_ir->function->n_arguments;
//    for (int i = 0; i < lox_ir->function->n_arguments; i++) {
//        put_u64_hash_table(&pr->v_register_by_local_number, i + 1, (void *) i);
//    }
//
//    return pr;
    return NULL;
}

static void free_phi_resolver(struct pr * pr) {
    free_arena(&pr->ps_allocator.arena);
    NATIVE_LOX_FREE(pr);
}

static struct v_register get_v_reg_by_ssa_name(struct pr * pr, struct ssa_name ssa_name, bool is_float) {
//    if (!contains_u64_hash_table(&pr->v_register_by_local_number, ssa_name.value.local_number)) {
//        struct v_register new_v_reg = alloc_v_register_lox_ir(pr->lox_ir, is_float);
//        put_u64_hash_table(&pr->v_register_by_local_number, ssa_name.value.local_number, (void *) new_v_reg.number);
//    }
//
//    uint64_t v_reg_as_number = (uint64_t) get_u64_hash_table(&pr->v_register_by_local_number, ssa_name.value.local_number);
//    struct v_register v_register = CREATE_V_REG(v_reg_as_number, ssa_name);
//    v_register.is_float_register = is_float;
//    return v_register;
}