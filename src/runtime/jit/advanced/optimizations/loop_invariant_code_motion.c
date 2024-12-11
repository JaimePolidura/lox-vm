#include "loop_invariant_code_motion.h"

extern void runtime_panic(char * format, ...);

struct licm {
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator allocator;
    struct stack_list pending; //Stack of struct ssa_control_node to check
};

struct licm * alloc_loop_invariant_code_motion(struct ssa_ir *);
void free_loop_invariant_code_motion(struct licm *);
static void initialization(struct licm *);
static bool initialization_block(struct ssa_block *, void *);
static void move_loop_invariants(struct licm *);
static bool can_control_node_be_checked_for_loop_invariant(struct ssa_control_node *node);
static void try_move_up_loop_invariant(struct licm *licm, struct ssa_control_node *node);
static bool is_data_node_loop_invariant(struct licm *, struct ssa_control_node *, struct ssa_data_node *);
static bool try_move_up_loop_invariant_data_node(struct ssa_data_node*, void**,struct ssa_data_node*, void*);
static void move_up_loop_invariant(struct licm *, struct ssa_control_node *, struct ssa_data_node *);

void perform_loop_invariant_code_motion(struct ssa_ir * ssa_ir) {
    struct licm * licm = alloc_loop_invariant_code_motion(ssa_ir);

    initialization(licm);
    move_loop_invariants(licm);

    free_loop_invariant_code_motion(licm);
}

static void move_loop_invariants(struct licm * licm) {
    while (!is_empty_stack_list(&licm->pending)) {
        struct ssa_control_node * pending_control_node = pop_stack_list(&licm->pending);

        if (can_control_node_be_checked_for_loop_invariant(pending_control_node)) {
            try_move_up_loop_invariant(licm, pending_control_node);
            push_stack_list(&licm->pending, pending_control_node);
        }
    }
}

static void initialization(struct licm * licm) {
    for_each_ssa_block(
            licm->ssa_ir->first_block,
            &licm->allocator.lox_allocator,
            licm,
            initialization_block
    );
}

struct try_move_up_loop_invariant_data_node_struct {
    struct ssa_control_node * control_node;
    struct licm * licm;
};

static void try_move_up_loop_invariant(struct licm * licm, struct ssa_control_node * node) {
    struct try_move_up_loop_invariant_data_node_struct consumer_struct = (struct try_move_up_loop_invariant_data_node_struct) {
        .control_node = node,
        .licm = licm,
    };

    for_each_data_node_in_control_node(
            node,
            &consumer_struct,
            SSA_DATA_NODE_OPT_PRE_ORDER | SSA_DATA_NODE_OPT_RECURSIVE | SSA_DATA_NODE_OPT_NOT_TERMINATORS,
            &try_move_up_loop_invariant_data_node
    );
}

static bool try_move_up_loop_invariant2_data_node(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * current_data_node,
        void * extra
) {
    struct try_move_up_loop_invariant_data_node_struct * consumer_struct = extra;

    if(is_data_node_loop_invariant(consumer_struct->licm, consumer_struct->control_node, current_data_node)) {
        move_up_loop_invariant(consumer_struct->licm, consumer_struct->control_node);
        push_stack_list(&consumer_struct->licm->pending, consumer_struct->control_node);
        //Dont keep scanning
        return false;
    }

    //Keep scanning
    return true;
}

//Only expected: CONDITIONAL_JUMP, SET_STRUCT_FIELD, SET_ARRAY_ELEMENT, PRINT, SET_GLOBAL, DEFINE_SSA_NAME, DATA, RETURN
static bool is_data_node_loop_invariant(
        struct licm * licm,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node
) {
    struct u64_set used_ssa_names = get_used_ssa_names_ssa_data_node(data_node, &licm->allocator.lox_allocator);
    struct ssa_block_loop_info * loop_info = get_loop_info_ssa_block(control_node->block, &licm->allocator.lox_allocator);

    bool all_loop_invariant = false;
    
    FOR_EACH_U64_SET_VALUE(used_ssa_names, used_ssa_name_u64) {
        struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_u64);

        if(contains_u64_set(&loop_info->modified_ssa_names, used_ssa_name.u16)) {
            all_loop_invariant = false;
            break;
        }
    }
    
    return all_loop_invariant;
}

static void move_up_loop_invariant(
        struct licm * licm,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node
) {
    struct ssa_name invariant_name = alloc_ssa_name_ssa_ir(licm->ssa_ir, 1, "loopInvariant");
}

static bool initialization_block(struct ssa_block * current_block, void * extra) {
    if(BELONGS_TO_LOOP_BODY_BLOCK(current_block)) {
        struct licm * licm = extra;

        for(struct ssa_control_node * current = current_block->first;;current = current->next) {
            push_stack_list(&licm->pending, current);

            if(current == current_block->last) {
                break;
            }
        }
    }

    return true;
}

static bool can_control_node_be_checked_for_loop_invariant(struct ssa_control_node * node) {
    if(node->block->is_loop_condition){
        return false;
    }

    switch (node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA:
        case SSA_CONTROL_NODE_TYPE_RETURN:
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME:
            return true;
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        default:
            return false;
    }
}

struct licm * alloc_loop_invariant_code_motion(struct ssa_ir * ssa_ir) {
    struct licm * licm = NATIVE_LOX_MALLOC(sizeof(struct licm));
    struct arena arena;
    init_arena(&arena);
    licm->ssa_ir = ssa_ir;
    licm->allocator = to_lox_allocator_arena(arena);
    init_stack_list(&licm->pending, &licm->allocator.lox_allocator);
    return licm;
}

void free_loop_invariant_code_motion(struct licm * licm) {
    free_arena(&licm->allocator.arena);
    NATIVE_LOX_FREE(licm);
}
