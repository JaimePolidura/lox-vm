#include "common_subexpression_elimination.h"

extern void runtime_panic(char * format, ...);

struct cse {
    struct u64_hash_table expressions_by_hash;
    struct arena_lox_allocator cse_allocator;
    struct lox_ir * lox_ir;
};

//Struct to hold information when performing subsexpression elimination in a data node.
//It is in the lox_ir_data_node method for_each_lox_ir_data_node
struct perform_cse_data_node {
    struct cse * cse;
    struct lox_ir_block * block;
    struct lox_ir_control_node * control_node;
};

struct subexpression {
    //Expression
    struct lox_ir_data_node * data_node;

    //Set when state is NOT_REUSABLE_SUBEXPRESSION. To be able to extract a subexpression
    struct lox_ir_control_node * control_node;
    struct lox_ir_block * block;
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

static struct cse * alloc_common_subexpression_elimination(struct lox_ir *);
static void free_common_subexpression_elimination(struct cse *);
void perform_cse_control_node(struct cse * cse, struct lox_ir_block *, struct lox_ir_control_node *);
static struct subexpression * alloc_not_reusable_subsexpression(struct perform_cse_data_node *, struct lox_ir_data_node *, void **);
static void extract_to_ssa_name(struct cse *, struct subexpression *);
static void reuse_subexpression(struct cse *, struct subexpression *, void **, struct lox_ir_control_node *);
static bool perform_cse_block(struct lox_ir_block *, void *);

void perform_common_subexpression_elimination(struct lox_ir * lox_ir) {
    struct cse * cse = alloc_common_subexpression_elimination(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            &cse->cse_allocator.lox_allocator,
            cse,
            LOX_IR_BLOCK_OPT_REPEATED,
            &perform_cse_block
    );

    free_common_subexpression_elimination(cse);
}

static bool perform_cse_block(struct lox_ir_block * block, void * extra) {
    struct cse * cse = extra;

    for (struct lox_ir_control_node * current_node = block->first;; current_node = current_node->next) {
        perform_cse_control_node(cse, block, current_node);

        if(current_node == block->last){
            break;
        }
    }

    return true;
}

bool perform_cse_data_node_consumer(
        struct lox_ir_data_node * _,
        void ** parent_child_ptr,
        struct lox_ir_data_node * current_data_node,
        void * extra
) {
    struct perform_cse_data_node * perform_cse_data_node = extra;
    struct lox_ir_control_node * current_control_node = perform_cse_data_node->control_node;
    struct cse * cse = perform_cse_data_node->cse;

    //These data node types won't be optimized
    if(current_data_node->type == LOX_IR_DATA_NODE_CALL ||
       current_data_node->type == LOX_IR_DATA_NODE_CONSTANT ||
       current_data_node->type == LOX_IR_DATA_NODE_GET_SSA_NAME ||
       current_data_node->type == LOX_IR_DATA_NODE_GET_STRUCT_FIELD ||
       current_data_node->type == LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT ||
       current_data_node->type == LOX_IR_DATA_NODE_INITIALIZE_ARRAY) {
        return true;
    }
    //We don't want to replace the main expression that is used in a loop condition
    if (current_control_node->type == LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP &&
        perform_cse_data_node->block->is_loop_condition &&
        current_data_node == GET_CONDITION_CONDITIONAL_JUMP_LOX_IR_NODE(current_control_node)) {
        return true;
    }

    uint64_t hash_current_node = hash_lox_ir_data_node(current_data_node);
    if (contains_u64_hash_table(&perform_cse_data_node->cse->expressions_by_hash, hash_current_node)) {
        struct subexpression * subexpression_to_reuse = get_u64_hash_table(
                &perform_cse_data_node->cse->expressions_by_hash, hash_current_node);

        if(!is_eq_lox_ir_data_node(subexpression_to_reuse->data_node, current_data_node,
                                   &cse->cse_allocator.lox_allocator)){
            return true;
        }
        if(!dominates_lox_ir_block(subexpression_to_reuse->block, perform_cse_data_node->block,
                                   &cse->cse_allocator.lox_allocator)){
            return true;
        }

        if(subexpression_to_reuse->state == NOT_REUSABLE_SUBEXPRESSION){
            extract_to_ssa_name(perform_cse_data_node->cse, subexpression_to_reuse);
        }

        reuse_subexpression(cse, subexpression_to_reuse, parent_child_ptr, current_control_node);
        return false;
    } else {
        struct subexpression * subexpression = alloc_not_reusable_subsexpression(
                perform_cse_data_node, current_data_node, parent_child_ptr);
        put_u64_hash_table(&cse->expressions_by_hash, hash_current_node, subexpression);

        return true;
    }
}

void perform_cse_control_node(
        struct cse * cse,
        struct lox_ir_block * block,
        struct lox_ir_control_node * control_node
) {
    switch (control_node->type) {
        case LOX_IR_CONTROL_NODE_DATA:
        case LOX_IR_CONTROL_NODE_RETURN:
        case LOX_IR_CONTROL_NODE_PRINT:
        case LOX_IR_CONTORL_NODE_SET_GLOBAL:
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD:
        case LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP:
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT:
        case LOX_IR_CONTROL_NODE_GUARD:
        case LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME: {
            struct perform_cse_data_node perform_cse_data_node = (struct perform_cse_data_node) {
                .control_node = control_node,
                .block = block,
                .cse = cse,
            };
            for_each_data_node_in_lox_ir_control(
                    control_node,
                    &perform_cse_data_node,
                    LOX_IR_DATA_NODE_OPT_PRE_ORDER,
                    perform_cse_data_node_consumer
            );
            break;
        }
        case LOX_IR_CONTROL_NODE_ENTER_MONITOR:
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR:
        case LOX_IR_CONTORL_NODE_SET_LOCAL:
        case LOX_IR_CONTROL_NODE_LOOP_JUMP:
            break;
    }
}

static void reuse_subexpression(
        struct cse * cse,
        struct subexpression * subexpression_to_reuse,
        void ** parent_data_node_ptr,
        struct lox_ir_control_node * control_node
) {
    struct ssa_name ssa_name_to_reuse = subexpression_to_reuse->reusable_ssa_name;

    struct lox_ir_data_get_ssa_name_node * get_ssa_name_to_reuse = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, LOX_IR_ALLOCATOR(cse->lox_ir)
    );
    get_ssa_name_to_reuse->data.produced_type = subexpression_to_reuse->data_node->produced_type;
    get_ssa_name_to_reuse->ssa_name = ssa_name_to_reuse;

    *parent_data_node_ptr = &get_ssa_name_to_reuse->data;
    add_ssa_name_use_lox_ir(cse->lox_ir, ssa_name_to_reuse, control_node);
}

static void extract_to_ssa_name(struct cse * cse, struct subexpression * subexpression_to_extract) {
    struct lox_ir_block * block = subexpression_to_extract->block;
    struct lox_ir_control_node * control_node = subexpression_to_extract->control_node;
    struct ssa_name reusable_ssa_name = alloc_ssa_name_lox_ir(cse->lox_ir, 1, "temp",
            subexpression_to_extract->block, subexpression_to_extract->data_node->produced_type);

    struct lox_ir_control_define_ssa_name_node * define_ssa_name_extracted = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME, struct lox_ir_control_define_ssa_name_node,
            subexpression_to_extract->block, LOX_IR_ALLOCATOR(cse->lox_ir)
    );
    define_ssa_name_extracted->value = subexpression_to_extract->data_node;
    define_ssa_name_extracted->ssa_name = reusable_ssa_name;
    add_before_control_node_lox_ir_block(block, control_node, &define_ssa_name_extracted->control);
    put_u64_hash_table(&cse->lox_ir->definitions_by_ssa_name, reusable_ssa_name.u16, define_ssa_name_extracted);

    struct lox_ir_data_get_ssa_name_node * get_extracted_ssa_name = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, LOX_IR_ALLOCATOR(cse->lox_ir)
    );
    get_extracted_ssa_name->data.produced_type = subexpression_to_extract->data_node->produced_type;
    get_extracted_ssa_name->ssa_name = reusable_ssa_name;

    *subexpression_to_extract->parent_ptr = &get_extracted_ssa_name->data;
    add_ssa_name_use_lox_ir(cse->lox_ir, reusable_ssa_name, control_node);

    subexpression_to_extract->state = REUSABLE_SUBEXPRESSION;
    subexpression_to_extract->reusable_ssa_name = reusable_ssa_name;
}

static struct subexpression * alloc_not_reusable_subsexpression(
        struct perform_cse_data_node * perform_cse_data_node,
        struct lox_ir_data_node * data_node,
        void ** parent_ptr
) {
    struct subexpression * subexpression = LOX_MALLOC(
            &perform_cse_data_node->cse->cse_allocator.lox_allocator, sizeof(struct subexpression)
    );
    struct lox_ir_control_node * control_node = perform_cse_data_node->control_node;

    subexpression->block = perform_cse_data_node->block;
    subexpression->control_node = control_node;
    subexpression->data_node = data_node;
    subexpression->parent_ptr = parent_ptr;

    if (control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME &&
        ((struct lox_ir_control_define_ssa_name_node *) control_node)->value == data_node){
        struct lox_ir_control_define_ssa_name_node * define_ssa_anme = (struct lox_ir_control_define_ssa_name_node *) control_node;

        subexpression->reusable_ssa_name = define_ssa_anme->ssa_name;
        subexpression->state = REUSABLE_SUBEXPRESSION;
    } else {
        subexpression->reusable_ssa_name = CREATE_SSA_NAME(0, 0);
        subexpression->state = NOT_REUSABLE_SUBEXPRESSION;
    }

    return subexpression;
}

static struct cse * alloc_common_subexpression_elimination(struct lox_ir * lox_ir) {
    struct cse * cse = NATIVE_LOX_MALLOC(sizeof(struct cse));
    struct arena arena;
    cse->lox_ir = lox_ir;
    init_arena(&arena);
    cse->cse_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&cse->expressions_by_hash, &cse->cse_allocator.lox_allocator);
    return cse;
}

static void free_common_subexpression_elimination(struct cse * cse) {
    free_arena(&cse->cse_allocator.arena);
    NATIVE_LOX_FREE(cse);
}