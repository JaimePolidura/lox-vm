#include "type_analysis.h"

struct ta {
    struct u64_hash_table ssa_type_by_ssa_name;
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator ta_allocator;
};

static struct ta * alloc_type_analysis(struct ssa_ir *);
static void free_type_analysis(struct ta *);
static void perform_type_analysis_block(struct ta *, struct ssa_block *);
static void perform_type_analysis_control(struct ta * ta, struct ssa_control_node *);
static bool perform_type_analysis_data(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static struct ssa_type perform_type_analysis_data_recursive(struct ta *, struct ssa_data_node *);

extern void runtime_panic(char * format, ...);

void perform_type_analysis(struct ssa_ir * ssa_ir) {
    struct ta * ta = alloc_type_analysis(ssa_ir);

    struct stack_list pending_blocks;
    init_stack_list(&pending_blocks, &ta->ta_allocator.lox_allocator);
    push_stack_list(&pending_blocks, ssa_ir->first_block);
    while(!is_empty_stack_list(&pending_blocks)) {
        struct ssa_block * current_block = pop_stack_list(&pending_blocks);

        perform_type_analysis_block(ta, current_block);

        switch (current_block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ: {
                push_stack_list(&pending_blocks, current_block->next_as.next);
            }
            case TYPE_NEXT_SSA_BLOCK_BRANCH: {
                push_stack_list(&pending_blocks, current_block->next_as.branch.true_branch);
                push_stack_list(&pending_blocks, current_block->next_as.branch.false_branch);
            }
        }
    }

    free_type_analysis(ta);
}

static void perform_type_analysis_block(struct ta * ta, struct ssa_block * block) {
    for(struct ssa_control_node * current_control = block->first;; current_control = current_control->next){
        perform_type_analysis_control(ta, current_control);

        if(current_control == block->last){
            break;
        }
    }
}

static void perform_type_analysis_control(struct ta * ta, struct ssa_control_node * control_node) {
    for_each_data_node_in_control_node(control_node, ta, SSA_DATA_NODE_OPT_PRE_ORDER, perform_type_analysis_data);
}

static bool perform_type_analysis_data(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct ta * ta = extra;
    data_node->produced_type = perform_type_analysis_data_recursive(ta, data_node);
    return false;
}

static struct ssa_type perform_type_analysis_data_recursive(struct ta * ta, struct ssa_data_node * data_node) {
    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call = (struct ssa_data_function_call_node *) data_node;
            for(int i = 0; i < call->n_arguments; i++){
                call->arguments[i]->produced_type = perform_type_analysis_data_recursive(ta, call->arguments[i]);
            }
            if(!call->is_native) {
                struct instruction_profile_data instruction_profile_data = get_instruction_profile_data_function(
                        ta->ssa_ir->function, data_node->original_bytecode);
                struct function_call_profile function_call_profile = instruction_profile_data.as.function_call;
                profile_data_type_t returned_type_profile = get_type_by_type_profile_data(function_call_profile.returned_type_profile);
                return CREATE_SSA_TYPE(returned_type_profile);
            } else {
                return CREATE_SSA_TYPE(call->native_function->returned_type);
            }
        }
            break;
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            return CREATE_SSA_TYPE(PROFILE_DATA_TYPE_ANY);
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            break;
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY:
            break;
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
            break;
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
            break;
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
            break;
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
            break;
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH:
            break;
        case SSA_DATA_NODE_TYPE_GUARD:
            break;
        case SSA_DATA_NODE_TYPE_PHI:
            break;
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
            break;

        case SSA_DATA_NODE_TYPE_GET_LOCAL:
            runtime_panic("Illegal code path");
    }
}

static struct ta * alloc_type_analysis(struct ssa_ir * ssa_ir) {
    struct ta * ta =  NATIVE_LOX_MALLOC(sizeof(struct ta));
    ta->ssa_ir = ssa_ir;
    struct arena arena;
    init_arena(&arena);
    ta->ta_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&ta->ssa_type_by_ssa_name, &ta->ta_allocator.lox_allocator);
    return ta;
}

static void free_type_analysis(struct ta * ta) {
    free_arena(&ta->ta_allocator.arena);
    NATIVE_LOX_FREE(ta);
}