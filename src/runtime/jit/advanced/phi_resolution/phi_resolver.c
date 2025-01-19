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
static bool resolve_phi_control(struct pr *, struct ssa_control_node *);
static v_register_t alloc_v_register(struct pr *);
static void replace_define_ssa_name_with_set_v_reg(struct pr*, struct ssa_control_define_ssa_name_node*);
static void replace_get_ssa_name_with_get_v_reg(struct pr*, struct ssa_data_get_ssa_name_node*);
static bool replace_phi_with_get_v_reg(struct pr*, struct ssa_data_phi_node*, struct ssa_control_define_ssa_name_node*);
static bool resolve_phi_data(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static bool all_predecessors_have_been_scanned(struct pr *, struct ssa_data_phi_node *);

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
        bool keep_scanning = resolve_phi_control(pr, current);

        if (current == block->last && keep_scanning) {
            break;
        }
    }

    return true;
}

//This functino will return false, if we don't have enough information to keep scannning, for example when we dont know
//all mappings to v_reg given a phi node, because we haven't scanned all predecessors blocks
static bool resolve_phi_control(struct pr * pr, struct ssa_control_node * control_node) {
    bool keep_scanning = for_each_data_node_in_control_node(
            control_node,
            pr,
            SSA_DATA_NODE_OPT_ANY_ORDER,
            resolve_phi_data
    );

    if (!keep_scanning) {
        return false;
    }

    if (control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
        replace_define_ssa_name_with_set_v_reg(pr, (struct ssa_control_define_ssa_name_node *) control_node);
    }

    return true;
}

static bool resolve_phi_data(
        struct ssa_data_node * _,
        void ** parent_field_ptr,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct pr * pr = extra;
    bool keep_scanning = true;

    if (data_node->type == SSA_DATA_NODE_TYPE_PHI) {
        struct ssa_data_node ** parent_ptr = (struct ssa_data_node **) parent_field_ptr;
        struct ssa_control_define_ssa_name_node * define_ssa_name = container_of(parent_ptr, struct ssa_control_define_ssa_name_node, value);
        keep_scanning = replace_phi_with_get_v_reg(pr, (struct ssa_data_phi_node *) data_node, define_ssa_name);
    } else if (data_node->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME) {
        replace_get_ssa_name_with_get_v_reg(pr, (struct ssa_data_get_ssa_name_node *) data_node);
    }

    return keep_scanning;
}

static void replace_get_ssa_name_with_get_v_reg(struct pr * pr, struct ssa_data_get_ssa_name_node * get_ssa_name) {
    v_register_t v_reg = (v_register_t) get_u64_hash_table(&pr->v_reg_by_ssa_name, get_ssa_name->ssa_name.u16);
    get_ssa_name->data.type = SSA_DATA_NODE_GET_V_REGISTER;
    struct ssa_data_get_v_register_node * get_v_reg = (struct ssa_data_get_v_register_node *) get_ssa_name;
    get_v_reg->v_register = v_reg;
}

//If all phi ssa names have not been mapped with a v_reg, the function will return faslse, meaning the it should stop scanning
static bool replace_phi_with_get_v_reg(
        struct pr * pr,
        struct ssa_data_phi_node * phi,
        struct ssa_control_define_ssa_name_node * define_ssa_name
) {
    if (!all_predecessors_have_been_scanned(pr, phi)) {
        return false;
    }

    v_register_t phi_result_v_reg = alloc_v_register(pr);

    FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_ssa_name) {
        void * ssa_name_definition_node = get_u64_hash_table(&pr->ssa_ir->ssa_definitions_by_ssa_name, current_ssa_name.u16);
        struct ssa_control_set_v_register_node * set_v_reg_phi_argument = ssa_name_definition_node;

        struct ssa_control_set_v_register_node * set_v_reg_phi_result = ALLOC_SSA_CONTROL_NODE(
                SSA_CONTROL_NODE_TYPE_SET_V_REGISTER, struct ssa_control_set_v_register_node, set_v_reg_phi_argument->control.block,
                SSA_IR_ALLOCATOR(pr->ssa_ir));

        struct ssa_data_get_v_register_node * get_v_reg_phi_argument = ALLOC_SSA_DATA_NODE(
                SSA_DATA_NODE_GET_V_REGISTER, struct ssa_data_get_v_register_node, NULL, SSA_IR_ALLOCATOR(pr->ssa_ir));
        get_v_reg_phi_argument->v_register = set_v_reg_phi_argument->v_register;
        get_v_reg_phi_argument->data.produced_type = phi->data.produced_type;

        set_v_reg_phi_result->v_register = phi_result_v_reg;
        set_v_reg_phi_result->value = &get_v_reg_phi_argument->data;
    }

    put_u64_hash_table(&pr->v_reg_by_ssa_name, define_ssa_name->ssa_name.u16, (void *) phi_result_v_reg);
    remove_control_node_ssa_block(define_ssa_name->control.block, &define_ssa_name->control);

    return true;
}

static bool all_predecessors_have_been_scanned(struct pr * pr, struct ssa_data_phi_node * phi) {
    FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_ssa_name) {
        if (!contains_u64_hash_table(&pr->v_reg_by_ssa_name, current_ssa_name.u16)) {
            //We haven't scanned all predecessors, stop scanning
            return false;
        }
    }
    return true;
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