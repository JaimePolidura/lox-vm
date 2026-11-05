#include "interference_graph.h"

struct igb {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator igb_allocator;
};

static struct interference_graph_node* create_or_get_node(struct igb*, struct ssa_name);
static void link_interference_graph_nodes(struct interference_graph_node*, struct interference_graph_node*);
static struct u64_set get_live_ssa_names_inside_live_range(struct igb*, struct liverange*);
static struct igb * alloc_interference_graph_builder(struct lox_ir*);
static void free_interference_graph_builder(struct igb*);
static bool get_live_ssa_names_between_blocks_consumer(struct lox_ir_block*, void*);
static struct u64_set get_live_ssa_names_inside_block(struct igb*,struct lox_ir_control_node*,struct lox_ir_control_node*);

void build_interference_graph(struct lox_ir * lox_ir, struct ptr_arraylist * liveranges) {
    struct igb * igb = alloc_interference_graph_builder(lox_ir);

    for (int i = 0; i < liveranges->in_use; i++) {
        struct liverange * liverange = liveranges->values[i];
        struct interference_graph_node * node = create_or_get_node(igb, liverange->from_v_reg_ssa_name);
        struct u64_set live_variables = get_live_ssa_names_inside_live_range(igb, liverange);

        FOR_EACH_U64_SET_VALUE(live_variables, uint64_t, other_live_ssa_name_u64) {
            struct ssa_name other_live_ssa_name = CREATE_SSA_NAME_FROM_U64(other_live_ssa_name_u64);
            struct interference_graph_node * other_node = create_or_get_node(igb, other_live_ssa_name);
            link_interference_graph_nodes(node, other_node);
        }
    }

    free_interference_graph_builder(igb);
}

static void link_interference_graph_nodes(struct interference_graph_node * a, struct interference_graph_node * b) {

}

static struct interference_graph_node * create_or_get_node(struct igb * igb, struct ssa_name ssa_name) {

}

struct get_live_ssa_names_between_blocks_struct {
    struct igb * igb;
    struct liverange * liverange;
    struct u64_set live_ssa_names;
};

static struct u64_set get_live_ssa_names_inside_live_range(struct igb * igb, struct liverange * liverange) {
    struct u64_set live_ssa_names;
    init_u64_set(&live_ssa_names, &igb->igb_allocator.lox_allocator);

    struct get_live_ssa_names_between_blocks_struct extra = (struct get_live_ssa_names_between_blocks_struct) {
        .live_ssa_names = live_ssa_names,
        .liverange = liverange
    };

    for_each_lox_ir_block(
        liverange->from_block,
        &igb->igb_allocator.lox_allocator,
        &extra,
        LOX_IR_BLOCK_OPT_NOT_REPEATED,
        &get_live_ssa_names_between_blocks_consumer
    );

    remove_u64_set(&extra.live_ssa_names, liverange->from_v_reg_ssa_name.u16);

    return extra.live_ssa_names;
}

static bool get_live_ssa_names_between_blocks_consumer(struct lox_ir_block * block, void * extra) {
    struct get_live_ssa_names_between_blocks_struct * extra_struct = extra;

    struct lox_ir_control_node * start_control_node_to_process = extra_struct->liverange->from_block == block
        ? (struct lox_ir_control_node *) extra_struct->liverange->from_control_node : block->first;
    struct lox_ir_control_node * end_control_node_to_process = extra_struct->liverange->to_block == block
        ? extra_struct->liverange->to_control_node->next : NULL;

    struct u64_set used_ssa_names_current_node = get_live_ssa_names_inside_block(
        extra_struct->igb, start_control_node_to_process, end_control_node_to_process);
    union_u64_set(&extra_struct->live_ssa_names, used_ssa_names_current_node);

    return block != extra_struct->liverange->to_block;
}

static struct u64_set get_live_ssa_names_inside_block(
    struct igb * igb,
    struct lox_ir_control_node * start, //Inclusive
    struct lox_ir_control_node * end //Exclusive, NULL if whole block should be processed
) {
    struct u64_set live_ssa_names;
    init_u64_set(&live_ssa_names, &igb->igb_allocator.lox_allocator);

    for (struct lox_ir_control_node * current = start; current != NULL && current != end; current = current->next) {
        struct u64_set used_ssa_names_current_node = get_used_ssa_names_lox_ir_control(current,
            &igb->igb_allocator.lox_allocator);
        union_u64_set(&live_ssa_names, used_ssa_names_current_node);
    }

    return live_ssa_names;
}

static struct igb * alloc_interference_graph_builder(struct lox_ir * lox_ir) {
    struct igb * igb = LOX_MALLOC(NATIVE_LOX_ALLOCATOR(), sizeof(struct igb));

    struct arena arena;
    init_arena(&arena);
    igb->lox_ir = lox_ir;
    igb->igb_allocator = to_lox_allocator_arena(arena);

    return igb;
}

static void free_interference_graph_builder(struct igb * igb) {
    free_arena(&igb->igb_allocator.arena);
    NATIVE_LOX_FREE(igb);
}
