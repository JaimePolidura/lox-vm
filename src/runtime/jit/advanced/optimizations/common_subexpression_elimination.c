#include "common_subexpression_elimination.h"

extern void runtime_panic(char * format, ...);

struct cse {
    struct u64_hash_table expressions_by_hash;
    struct arena_lox_allocator cse_allocator;
    struct ssa_ir * ssa_ir;
};

//Struct to hold information when performing subsexpression elimination in a data node.
//It is in the ssa_data_node method for_each_ssa_data_node
struct perform_cse_data_node {
    struct cse * cse;
    struct ssa_block * block;
    struct ssa_control_node * control_node;
};

struct subexpression {
    struct ssa_data_node * data_node;

    //Set when state is NOT_REUSABLE_SUBEXPRESSION. To be able to extract a subexpression
    struct ssa_control_node * control_node;
    struct ssa_block * block;
    void ** parent_ptr;
    //Set when state is REUSABLE_SUBEXPRESSION. Other nodes will be able to reuse the expression by this ssa name
    struct ssa_name reusable_ssa_name;

    enum {
        //The value resides in a node data flow graph. Exmaple: return phi(i0, i1)
        //To be used it needs to be extracted to a ssa name.
        NOT_REUSABLE_SUBEXPRESSION,
        //The value has been extracted to a ssa name. Example: i2 = phi(i0, i1); return i2;
        //so that the value can be reused.
        REUSABLE_SUBEXPRESSION
    } state;
};

static struct cse * alloc_common_subexpression_elimination(struct ssa_ir *);
static void free_common_subexpression_elimination(struct cse *);
void perform_cse(struct cse *);
void perform_cse_block(struct cse * cse, struct ssa_block *);
void perform_cse_control_node(struct cse * cse, struct ssa_block *, struct ssa_control_node *);
static struct subexpression * alloc_not_reusable_subsexpression(struct perform_cse_data_node *, struct ssa_data_node *, void **);
static void extract_to_ssa_name(struct cse *, struct subexpression *);
static void reuse_subexpression(struct cse *, struct subexpression *, void **, struct ssa_control_node *);

void perform_common_subexpression_elimination(struct ssa_ir * ssa_ir) {
    struct cse * cse = alloc_common_subexpression_elimination(ssa_ir);
    perform_cse(cse);
    free_common_subexpression_elimination(cse);
}

void perform_cse(struct cse * cse) {
    struct stack_list pending;
    init_stack_list(&pending, &cse->cse_allocator.lox_allocator);
    push_stack_list(&pending, cse->ssa_ir->first_block);

    while(!is_empty_stack_list(&pending)){
        struct ssa_block * current_block = pop_stack_list(&pending);

        perform_cse_block(cse, current_block);

        switch (current_block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_stack_list(&pending, current_block->next_as.next);
                break;
            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_stack_list(&pending, current_block->next_as.branch.false_branch);
                push_stack_list(&pending, current_block->next_as.branch.true_branch);
                break;
            default:
                break;
        }
    }
}

void perform_cse_block(struct cse * cse, struct ssa_block * block) {
    for(struct ssa_control_node * current_node = block->first;; current_node = current_node->next){
        perform_cse_control_node(cse, block, current_node);

        if(current_node == block->last){
            break;
        }
    }
}

void perform_cse_data_node_consumer(
        struct ssa_data_node * _,
        void ** parent_child_ptr,
        struct ssa_data_node * current_data_node,
        void * extra
) {
    struct perform_cse_data_node * perform_cse_data_node = extra;
    struct ssa_control_node * current_control_node = perform_cse_data_node->control_node;
    struct cse * cse = perform_cse_data_node->cse;

    //These data node types won't be optimized
    if(current_data_node->type == SSA_DATA_NODE_TYPE_CALL ||
            current_data_node->type == SSA_DATA_NODE_TYPE_CONSTANT ||
            current_data_node->type == SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD ||
            current_data_node->type == SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT ||
            current_data_node->type == SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY) {
        return;
    }
    //We don't want to replace the main expression that is used in a loop condition
    if (current_control_node->type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP &&
        perform_cse_data_node->block->loop_condition &&
        current_data_node == GET_CONDITION_CONDITIONAL_JUMP_SSA_NODE(current_control_node)) {
        return;
    }

    uint64_t hash_current_node = hash_ssa_data_node(current_data_node);
    if (contains_u64_hash_table(&perform_cse_data_node->cse->expressions_by_hash, hash_current_node)) {
        struct subexpression * subexpression_to_reuse = get_u64_hash_table(
                &perform_cse_data_node->cse->expressions_by_hash, hash_current_node);

        if(!is_eq_ssa_data_node(subexpression_to_reuse->data_node, current_data_node)){
            return;
        }
        if(!dominates_ssa_block(subexpression_to_reuse->block, perform_cse_data_node->block, &cse->cse_allocator.lox_allocator)){
            return;
        }

        if(subexpression_to_reuse->state == NOT_REUSABLE_SUBEXPRESSION){
            extract_to_ssa_name(perform_cse_data_node->cse, subexpression_to_reuse);
        }

        reuse_subexpression(cse, subexpression_to_reuse, parent_child_ptr, current_control_node);
    } else {
        struct subexpression * subexpression = alloc_not_reusable_subsexpression(
                perform_cse_data_node, current_data_node, parent_child_ptr);
        put_u64_hash_table(&cse->expressions_by_hash, hash_current_node, subexpression);
    }
}

void perform_cse_control_node(
        struct cse * cse,
        struct ssa_block * block,
        struct ssa_control_node * control_node
) {
    switch (control_node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA:
        case SSA_CONTROL_NODE_TYPE_RETURN:
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME: {
            struct perform_cse_data_node perform_cse_data_node = (struct perform_cse_data_node) {
                .control_node = control_node,
                .block = block,
                .cse = cse,
            };
            for_each_data_node_in_control_node(
                    control_node,
                    &perform_cse_data_node,
                    SSA_DATA_NODE_OPT_RECURSIVE | SSA_DATA_NODE_OPT_PRE_ORDER,
                    perform_cse_data_node_consumer
            );
            break;
        }
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
            break;
    }
}

static void reuse_subexpression(
        struct cse * cse,
        struct subexpression * subexpression_to_reuse,
        void ** parent_data_node_ptr,
        struct ssa_control_node * ssa_control_node
) {
    struct ssa_name ssa_name_to_reuse = subexpression_to_reuse->reusable_ssa_name;

    struct ssa_data_get_ssa_name_node * get_ssa_name_to_reuse = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, &cse->cse_allocator.lox_allocator
    );
    get_ssa_name_to_reuse->ssa_name = ssa_name_to_reuse;
    *parent_data_node_ptr = &get_ssa_name_to_reuse->data;
    add_ssa_name_use_ssa_ir(cse->ssa_ir, ssa_name_to_reuse, ssa_control_node);
}

static void extract_to_ssa_name(struct cse * cse, struct subexpression * subexpression_to_extract) {
    struct ssa_block * block = subexpression_to_extract->block;
    struct ssa_control_node * ssa_control_node = subexpression_to_extract->control_node;
    struct ssa_name extracted_ssa_name = alloc_ssa_name_ssa_ir(cse->ssa_ir, 1, "temp");

    struct ssa_control_define_ssa_name_node * define_ssa_name_extracted = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node,
                    subexpression_to_extract->block, &cse->cse_allocator.lox_allocator
    );
    define_ssa_name_extracted->value = subexpression_to_extract->data_node;
    define_ssa_name_extracted->ssa_name = extracted_ssa_name;
    add_before_control_node_ssa_block(block, ssa_control_node, &define_ssa_name_extracted->control);
    put_u64_hash_table(&cse->ssa_ir->ssa_definitions_by_ssa_name, extracted_ssa_name.u16, define_ssa_name_extracted);

    struct ssa_data_get_ssa_name_node * get_extracted_ssa_name = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, &cse->cse_allocator.lox_allocator
    );
    get_extracted_ssa_name->ssa_name = extracted_ssa_name;
    *subexpression_to_extract->parent_ptr = &get_extracted_ssa_name->data;
    add_ssa_name_use_ssa_ir(cse->ssa_ir, extracted_ssa_name, ssa_control_node);

    subexpression_to_extract->state = REUSABLE_SUBEXPRESSION;
    subexpression_to_extract->reusable_ssa_name = extracted_ssa_name;
}

static struct subexpression * alloc_not_reusable_subsexpression(
        struct perform_cse_data_node * perform_cse_data_node,
        struct ssa_data_node * ssa_data_node,
        void ** parent_ptr
) {
    struct subexpression * subexpression = LOX_MALLOC(
            &perform_cse_data_node->cse->cse_allocator.lox_allocator, sizeof(struct subexpression)
    );

    subexpression->control_node = perform_cse_data_node->control_node;
    subexpression->block = perform_cse_data_node->block;
    subexpression->state = NOT_REUSABLE_SUBEXPRESSION;
    subexpression->reusable_ssa_name = CREATE_SSA_NAME(0, 0);
    subexpression->data_node = ssa_data_node;
    subexpression->parent_ptr = parent_ptr;

    return subexpression;
}

static struct cse * alloc_common_subexpression_elimination(struct ssa_ir * ssa_ir) {
    struct cse * cse = NATIVE_LOX_MALLOC(sizeof(struct cse));
    struct arena arena;
    cse->ssa_ir = ssa_ir;
    init_arena(&arena);
    cse->cse_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&cse->expressions_by_hash, &cse->cse_allocator.lox_allocator);
    return cse;
}

static void free_common_subexpression_elimination(struct cse * cse) {
    free_arena(&cse->cse_allocator.arena);
    NATIVE_LOX_FREE(cse);
}