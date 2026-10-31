#include "interference_graph.h"

struct igb {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator igb_allocator;
};

static struct interference_graph_node* create_or_get_node(struct igb*, struct ssa_name);
static void link_interference_graph_nodes(struct interference_graph_node*, struct interference_graph_node*);
static struct ptr_arraylist get_live_ssa_names_inside_live_range(struct igb*, struct liverange*);
static struct igb * alloc_interference_graph_builder(struct lox_ir*);
static void free_interference_graph_builder(struct igb*);

void build_interference_graph(struct lox_ir * lox_ir, struct ptr_arraylist * liveranges) {
    struct igb * igb = alloc_interference_graph_builder(lox_ir);

    for (int i = 0; i < liveranges->in_use; i++) {
        struct liverange * liverange = liveranges->values[i];
        struct interference_graph_node * node = create_or_get_node(igb, liverange->from_v_reg_ssa_name);
        struct ptr_arraylist live_variables = get_live_ssa_names_inside_live_range(igb, liverange);

        for (int i = 0; i < live_variables.in_use; i++) {
            struct ssa_name other_live_ssa_name = CREATE_SSA_NAME_FROM_U64(live_variables.values[i]);
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

static struct ptr_arraylist get_live_ssa_names_inside_live_range(struct igb * igb, struct liverange * liverange) {

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
