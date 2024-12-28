#include "unboxing_insertion.h"

struct ui {
    struct ssa_ir * ssa_ir;
    struct u64_hash_table is_unboxed_by_ssa_name;
    struct arena_lox_allocator ui_allocator;
};

static struct ui * alloc_unbox_insertion(struct ssa_ir *);
static void free_unbox_insertion(struct ui*);
static bool control_requires_boxed_input(struct ssa_control_node *control);

static bool perform_unboxing_insertion_block(struct ssa_block *, void *);
static void perform_unboxing_insertion_control(struct ui *, struct ssa_block *, struct ssa_control_node *);
static bool perform_unboxing_insertion_data_consumer(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static void perform_unboxing_insertion_data(struct ui*, struct ssa_data_node*,  void**, bool should_produce_boxed);

void perform_unboxing_insertion(struct ssa_ir * ssa_ir) {
    struct ui * ui = alloc_unbox_insertion(ssa_ir);

    for_each_ssa_block(
            ssa_ir->first_block,
            &ui->ui_allocator.lox_allocator,
            ui,
            SSA_BLOCK_OPT_NOT_REPEATED,
            &perform_unboxing_insertion_block
    );

    free_unbox_insertion(ui);
}

static bool perform_unboxing_insertion_block(struct ssa_block * current_block, void * extra) {
    struct ui * ui = extra;

    for(struct ssa_control_node * current_control = current_block->first;; current_control = current_control->next){
        perform_unboxing_insertion_control(ui, current_block, current_control);

        if (current_control == current_block->last) {
            break;
        }
    }

    return true;
}

struct perform_unboxing_insertion_data_struct {
    bool should_produced_boxed_result;
    struct ui * ui;
};

static void perform_unboxing_insertion_control(
        struct ui * ui,
        struct ssa_block * current_block,
        struct ssa_control_node * current_control
) {
    struct perform_unboxing_insertion_data_struct consumer_struct = (struct perform_unboxing_insertion_data_struct) {
        .should_produced_boxed_result = control_requires_boxed_input(current_control),
        .ui = ui,
    };

    for_each_data_node_in_control_node(
            current_control,
            &consumer_struct,
            SSA_DATA_NODE_OPT_PRE_ORDER,
            &perform_unboxing_insertion_data_consumer
    );
}

static bool perform_unboxing_insertion_data_consumer(
        struct ssa_data_node * parent,
        void ** parent_child_ptr,
        struct ssa_data_node * child,
        void * extra
) {
    struct perform_unboxing_insertion_data_struct * consumer_struct = extra;
    bool should_produed_boxed_result = consumer_struct->should_produced_boxed_result;
    perform_unboxing_insertion_data(consumer_struct->ui, child, parent_child_ptr, should_produed_boxed_result);

    //Only iterate parent
    return false;
}

static void perform_unboxing_insertion_data(
        struct ui * ui,
        struct ssa_data_node * data_node,
        void ** parent_data_node_ptr,
        bool should_produce_boxed
) {
    struct u64_set children = get_children_ssa_data_node(data_node, &ui->ui_allocator.lox_allocator);
}

static bool control_requires_boxed_input(struct ssa_control_node * control) {
    switch (control->type) {
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_RETURN:
            return true;
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTROL_NODE_TYPE_DATA:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_GUARD:
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME:
            return false;
    }
}

static struct ui * alloc_unbox_insertion(struct ssa_ir * ssa_ir) {
    struct ui * ui =  NATIVE_LOX_MALLOC(sizeof(struct ui));
    ui->ssa_ir = ssa_ir;
    struct arena arena;
    init_arena(&arena);
    ui->ui_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&ui->is_unboxed_by_ssa_name, &ui->ui_allocator.lox_allocator);

    return ui;
}

static void free_unbox_insertion(struct ui * ui) {
    free_arena(&ui->ui_allocator.arena);
    NATIVE_LOX_FREE(ui);
}