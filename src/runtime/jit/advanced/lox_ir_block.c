#include "lox_ir_block.h"

static struct lox_ir_block_loop_info * create_loop_info_lox_ir_block(struct lox_ir_block*,struct lox_allocator*);
static void record_new_node_information_of_block(struct lox_ir_block *, struct lox_ir_control_node *);
static bool create_loop_info_lox_ir_block_consumer(struct lox_ir_block *current_block, void *extra);
static struct u64_set get_blocks_to_remove(struct lox_ir_block *);


void for_each_lox_ir_block(
        struct lox_ir_block * start_block,
        struct lox_allocator * allocator,
        void * extra,
        long options,
        lox_ir_block_consumer_t consumer
) {
    struct stack_list pending;
    init_stack_list(&pending, allocator);
    push_stack_list(&pending, start_block);

    struct u64_set visited_blocks;
    if (IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED)) {
        init_u64_set(&visited_blocks, allocator);
    }

    while(!is_empty_stack_list(&pending)){
        struct lox_ir_block * current_block = pop_stack_list(&pending);

        if(IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED) && contains_u64_set(&visited_blocks, (uint64_t) current_block)){
            continue;
        }
        if(IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED) && !is_subset_u64_set(visited_blocks, current_block->predecesors)){
            continue;
        }

        //Dont continue iterating
        bool continue_scanning_from_this_block = consumer(current_block, extra);
        if(IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED)){
            add_u64_set(&visited_blocks, (uint64_t) current_block);
        }
        if(!continue_scanning_from_this_block){
            continue;
        }

        switch (current_block->type_next) {
            case TYPE_NEXT_LOX_IR_BLOCK_SEQ:
                push_stack_list(&pending, current_block->next_as.next);
                break;
            case TYPE_NEXT_LOX_IR_BLOCK_BRANCH:
                push_stack_list(&pending, current_block->next_as.branch.false_branch);
                push_stack_list(&pending, current_block->next_as.branch.true_branch);
                break;
            default:
                break;
        }
    }

    free_stack_list(&pending);
    if(IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED)){
        free_u64_set(&visited_blocks);
    }
}

struct lox_ir_block * alloc_lox_ir_block(struct lox_allocator * allocator) {
    struct lox_ir_block * block = LOX_MALLOC(allocator, sizeof(struct lox_ir_block));
    init_lox_ir_block(block, allocator);
    return block;
}

void init_lox_ir_block(struct lox_ir_block * block, struct lox_allocator * allocator) {
    memset(block, 0, sizeof(struct lox_ir_block));
    init_u64_set(&block->predecesors, allocator);
    init_u64_set(&block->defined_ssa_names, allocator);
}

type_next_lox_ir_block_t get_type_next_lox_ir_block(struct lox_ir_control_node * node) {
    switch(node->type) {
        case LOX_IR_CONTROL_NODE_PRINT:
        case LOX_IR_CONTROL_NODE_ENTER_MONITOR:
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR:
        case LOX_IR_CONTORL_NODE_SET_GLOBAL:
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT:
        case LOX_IR_CONTORL_NODE_SET_LOCAL:
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD:
        case LOX_IR_CONTROL_NODE_GUARD:
        case LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME:
        case LOX_IR_CONTROL_NODE_DATA: {
            return TYPE_NEXT_LOX_IR_BLOCK_SEQ;
        }
        case LOX_IR_CONTROL_NODE_RETURN: {
            return TYPE_NEXT_LOX_IR_BLOCK_NONE;
        }
        case LOX_IR_CONTROL_NODE_LOOP_JUMP: {
            return TYPE_NEXT_LOX_IR_BLOCK_LOOP;
        }
        case LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP: {
            return TYPE_NEXT_LOX_IR_BLOCK_BRANCH;
        }
    }
}

bool is_emtpy_lox_ir_block(struct lox_ir_block * block) {
    return block->first == NULL && block->last == NULL;
}

void replace_block_lox_ir_block(struct lox_ir_block * old_block, struct lox_ir_block * new_block) {
    reset_loop_info(new_block);

    FOR_EACH_U64_SET_VALUE(old_block->predecesors, struct lox_ir_block *, predecesor_node) {
        switch (predecesor_node->type_next) {
            case TYPE_NEXT_LOX_IR_BLOCK_LOOP: {
                if (predecesor_node->next_as.loop == old_block) {
                    predecesor_node->next_as.loop = new_block;
                }
                break;
            }
            case TYPE_NEXT_LOX_IR_BLOCK_SEQ: {
                if(predecesor_node->next_as.next == old_block) {
                    predecesor_node->next_as.next = new_block;
                }
                break;
            }
            case TYPE_NEXT_LOX_IR_BLOCK_BRANCH: {
                if(predecesor_node->next_as.branch.true_branch == old_block){
                    predecesor_node->next_as.branch.true_branch = new_block;
                }
                if(predecesor_node->next_as.branch.false_branch == old_block){
                    predecesor_node->next_as.branch.false_branch = new_block;
                }
                break;
            }
            case TYPE_NEXT_LOX_IR_BLOCK_NONE:
                break;
        }
    }
}

struct lox_ir_block_loop_info * get_loop_info_lox_ir_block(struct lox_ir_block * block, struct lox_allocator * allocator) {
    if(!BELONGS_TO_LOOP_BODY_BLOCK(block)){
        return NULL;
    }
    if(block->is_loop_condition && block->loop_info != NULL){
        return block->loop_info;
    }
    if(!block->is_loop_condition && block->loop_condition_block->loop_info != NULL){
        return block->loop_condition_block->loop_info;
    }

    struct lox_ir_block * loop_condition_block = block->is_loop_condition ? block : block->loop_condition_block;
    struct lox_ir_block_loop_info * loop_info = create_loop_info_lox_ir_block(loop_condition_block, allocator);

    loop_condition_block->loop_info = loop_info;

    return loop_info;
}

static struct lox_ir_block_loop_info * create_loop_info_lox_ir_block(struct lox_ir_block * loop_condition_block, struct lox_allocator * allocator) {
    struct lox_ir_block_loop_info * lox_ir_block_loop_info = LOX_MALLOC(allocator, sizeof(struct lox_ir_block_loop_info));
    loop_condition_block->loop_info = lox_ir_block_loop_info;
    init_u8_set(&lox_ir_block_loop_info->modified_local_numbers);
    lox_ir_block_loop_info->condition_block = loop_condition_block;

    for_each_lox_ir_block(
            loop_condition_block,
            allocator,
            lox_ir_block_loop_info,
            LOX_IR_BLOCK_OPT_REPEATED,
            create_loop_info_lox_ir_block_consumer
    );

    return lox_ir_block_loop_info;
}

static bool create_loop_info_lox_ir_block_consumer(struct lox_ir_block * current_block, void * extra) {
    struct lox_ir_block_loop_info * block_loop_info = extra;

    if(current_block->is_loop_condition){
        //Current block is the loop condtiion block that we want to scan. We want to keep scanning from it
        return true;
    }

    if (current_block->loop_condition_block != block_loop_info->condition_block) {
        //Dont keep scanning from current_block
        return false;
    }

    for(struct lox_ir_control_node * current_control_node = current_block->first;; current_control_node = current_control_node->next) {
        if(current_control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
            struct lox_ir_control_define_ssa_name_node * define_node = (struct lox_ir_control_define_ssa_name_node *) current_control_node;
            add_u8_set(&block_loop_info->modified_local_numbers, define_node->ssa_name.value.local_number);
        } else if(current_control_node->type == LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT){
            struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) current_control_node;
            struct lox_ir_data_node * array_instance = set_array_element->array;

            if (array_instance->type == LOX_IR_DATA_NODE_GET_SSA_NAME) {
                struct lox_ir_data_get_ssa_name_node * get_array_instance = (struct lox_ir_data_get_ssa_name_node *) array_instance;
                add_u8_set(&block_loop_info->modified_local_numbers, get_array_instance->ssa_name.value.local_number);
            }
        }

        if(current_control_node == current_block->last){
            break;
        }
    }

    return true;
}

void reset_loop_info(struct lox_ir_block * block) {
    block->loop_info = NULL;
    if(!block->is_loop_condition && block->loop_condition_block != NULL){
        block->loop_condition_block->loop_info = NULL;
    }
    if(block->is_loop_condition){
        block->loop_info = NULL;
    }
}

static void record_new_node_information_of_block(
        struct lox_ir_block * block,
        struct lox_ir_control_node * new_block_node
) {
    if(new_block_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME){
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) new_block_node;
        add_u64_set(&block->defined_ssa_names, define->ssa_name.u16);
    }
}

void record_removed_node_information_of_block(
        struct lox_ir_block * block,
        struct lox_ir_control_node * new_block_node
) {
    if(new_block_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME){
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) new_block_node;
        remove_u64_set(&block->defined_ssa_names, define->ssa_name.u16);
    }
}

struct u64_set get_successors_lox_ir_block(struct lox_ir_block * block, struct lox_allocator * allocator) {
    struct u64_set children;
    init_u64_set(&children, allocator);

    if(block->type_next == TYPE_NEXT_LOX_IR_BLOCK_SEQ){
        add_u64_set(&children, (uint64_t) block->next_as.next);
    } else if(block->type_next == TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
        add_u64_set(&children, (uint64_t) block->next_as.branch.false_branch);
        add_u64_set(&children, (uint64_t) block->next_as.branch.true_branch);
    }

    return children;
}

int get_index_control_lox_ir_block(
        struct lox_ir_block * block,
        struct lox_ir_control_node * target
) {
    int current_index = 0;

    for (struct lox_ir_control_node * current = block->first;;target = target->next, current_index++) {
        if (current == target) {
            return current_index;
        }
    }

    return -1;
}