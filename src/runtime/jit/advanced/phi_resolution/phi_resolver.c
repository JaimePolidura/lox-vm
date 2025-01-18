#include "phi_resolver.h"

struct pr {
    struct u64_hash_table v_reg_by_ssa_name;
    v_register_t last_v_regiser_allocated;

    struct arena_lox_allocator ps_allocator;
    struct ssa_ir * ssa_ir;
};

static bool resolve_phi_block(struct ssa_block *, void *);

static struct pr * alloc_phi_resolver(struct ssa_ir *);
static void free_phi_resolver(struct pr *);
static void resolve_phi_control(struct pr *, struct ssa_control_node *);
static v_register_t alloc_v_register(struct pr *);
static void replace_define_ssa_name_with_set_v_reg(struct pr*, struct ssa_control_define_ssa_name_node*);
static void replace_get_ssa_name_with_get_v_reg(struct pr*, struct ssa_data_get_ssa_name_node*);
static void replace_phi_with_get_v_reg(struct pr*, struct ssa_data_phi_node*);
static bool resolve_phi_data(struct ssa_data_node*, void**, struct ssa_data_node*, void*);

void resolve_phi(struct ssa_ir * ssa_ir) {
    struct pr * phi_resolver = alloc_phi_resolver(ssa_ir);

    for_each_ssa_block(
            ssa_ir->first_block,
            NATIVE_LOX_ALLOCATOR(),
            phi_resolver,
            SSA_BLOCK_OPT_NOT_REPEATED,
            resolve_phi_block
    );

    free_phi_resolver(phi_resolver);
}

static bool resolve_phi_block(struct ssa_block * block, void * extra) {
    struct pr * pr = extra;

    for (struct ssa_control_node * current = block->first;; current = current->next) {
        resolve_phi_control(pr, current);

        if (current == block->last) {
            break;
        }
    }

    return true;
}

static void resolve_phi_control(struct pr * pr, struct ssa_control_node * control_node) {
    if (control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
        replace_define_ssa_name_with_set_v_reg(pr, (struct ssa_control_define_ssa_name_node *) control_node);
    }

    for_each_data_node_in_control_node(
            control_node,
            pr,
            SSA_DATA_NODE_OPT_ANY_ORDER,
            resolve_phi_data
    );
}

static bool resolve_phi_data(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct pr * pr = extra;

    if (data_node->type == SSA_DATA_NODE_TYPE_PHI) {
        replace_phi_with_get_v_reg(pr, (struct ssa_data_phi_node *) data_node);
    } else if (data_node->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME) {
        replace_get_ssa_name_with_get_v_reg(pr, (struct ssa_data_get_ssa_name_node *) data_node);
    }

    return true;
}

static void replace_get_ssa_name_with_get_v_reg(struct pr * pr, struct ssa_data_get_ssa_name_node * get_ssa_name) {
    v_register_t v_reg = (v_register_t) get_u64_hash_table(&pr->v_reg_by_ssa_name, get_ssa_name->ssa_name.u16);
    get_ssa_name->data.type = SSA_DATA_NODE_GET_V_REGISTER;
    struct ssa_data_get_v_register_node * get_v_reg = (struct ssa_data_get_v_register_node *) get_ssa_name;
    get_v_reg->v_register = v_reg;
}

static void replace_phi_with_get_v_reg(struct pr * pr, struct ssa_data_phi_node * phi) {

}

static void replace_define_ssa_name_with_set_v_reg(struct pr * pr, struct ssa_control_define_ssa_name_node * define_ssa_name) {
    struct ssa_data_node * value = define_ssa_name->value;
    struct ssa_name ssa_name = define_ssa_name->ssa_name;

    define_ssa_name->control.type = SSA_CONTROL_NODE_TYPE_SET_V_REGISTER;
    v_register_t new_v_reg = alloc_v_register(pr);
    struct ssa_control_set_v_register_node * set_v_reg = (struct ssa_control_set_v_register_node *) define_ssa_name;
    set_v_reg->v_register = new_v_reg;
    set_v_reg->value = value;
    put_u64_hash_table(&pr->v_reg_by_ssa_name, ssa_name.u16, (void *) new_v_reg);
}

static v_register_t alloc_v_register(struct pr * pr) {
    return pr->last_v_regiser_allocated++;
}

static struct pr * alloc_phi_resolver(struct ssa_ir * ssa_ir) {
    struct pr * pr = NATIVE_LOX_MALLOC(sizeof(struct pr));
    struct arena arena;
    init_arena(&arena);
    pr->ssa_ir = ssa_ir;
    pr->ps_allocator = to_lox_allocator_arena(arena);
    pr->last_v_regiser_allocated = 0;
    init_u64_hash_table(&pr->v_reg_by_ssa_name, &pr->ps_allocator.lox_allocator);

    return pr;
}

static void free_phi_resolver(struct pr * pr) {
    free_arena(&pr->ps_allocator.arena);
    NATIVE_LOX_FREE(pr);
}