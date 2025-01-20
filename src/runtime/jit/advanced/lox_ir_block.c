#include "lox_ir_block.h"

static void reset_loop_info(struct lox_ir_block *);
static bool create_loop_info_lox_ir_block_consumer(struct lox_ir_block *current_block, void *extra);
static void record_new_node_information_of_block(struct lox_ir_block *, struct lox_ir_control_node *);
static void record_removed_node_information_of_block(struct lox_ir_block *, struct lox_ir_control_node *);
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
    if(IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED)){
        init_u64_set(&visited_blocks, allocator);
    }

    while(!is_empty_stack_list(&pending)){
        struct lox_ir_block * current_block = pop_stack_list(&pending);

        if(IS_FLAG_SET(options, LOX_IR_BLOCK_OPT_NOT_REPEATED) && contains_u64_set(&visited_blocks, (uint64_t) current_block)){
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

void remove_control_node_lox_ir_block(
        struct lox_ir_block * lox_ir_block,
        struct lox_ir_control_node * node_to_remove
) {
    record_removed_node_information_of_block(lox_ir_block, node_to_remove);
    reset_loop_info(lox_ir_block);

    //The block has only 1 control control_node
    if(lox_ir_block->first == lox_ir_block->last){
        lox_ir_block->first = lox_ir_block->last = NULL;
        return;
    }
    //We are removing the first control_node
    if(lox_ir_block->first == node_to_remove) {
        lox_ir_block->first = node_to_remove->next;
    }
    //We are removing the last control_node
    if(lox_ir_block->last == node_to_remove){
        lox_ir_block->last = node_to_remove->prev;
    }
    //There is only one control_node remaining
    if(lox_ir_block->last == lox_ir_block->first){
        lox_ir_block->first->next = NULL;
        lox_ir_block->first->prev = NULL;
        return;
    }
    //Unlink the control_node from the control control_node linkedlist in a block
    if (node_to_remove->prev != NULL) {
        node_to_remove->prev->next = node_to_remove->next;
    }
    if(node_to_remove->next != NULL){
        node_to_remove->next->prev = node_to_remove->prev;
    }
}

void add_last_control_node_lox_ir_block(struct lox_ir_block * block, struct lox_ir_control_node * node) {
    record_new_node_information_of_block(block, node);
    reset_loop_info(block);

    if(block->first == NULL){
        block->first = node;
    }
    if(block->last != NULL) {
        block->last->next = node;
        node->prev = block->last;
    }

    block->last = node;
}

void add_after_control_node_lox_ir_block(
        struct lox_ir_block * block,
        struct lox_ir_control_node * after,
        struct lox_ir_control_node * new
) {
    record_new_node_information_of_block(block, new);
    reset_loop_info(block);

    if (block->last == after) {
        block->last = new;
    }
    if (after->next != NULL) {
        new->next = after->next;
        after->next->prev = new;
    }

    new->prev = after;
    after->next = new;
}

void add_before_control_node_lox_ir_block(
        struct lox_ir_block * block,
        struct lox_ir_control_node * before,
        struct lox_ir_control_node * new
) {
    record_new_node_information_of_block(block, new);
    reset_loop_info(block);

    if(block->first == before){
        block->first = new;
    }

    if(before->prev != NULL){
        new->prev = before->prev;
        before->prev->next = new;
    }

    new->next = before;
    before->prev = new;
}

bool is_emtpy_ssa_block(struct lox_ir_block * block) {
    return block->first == NULL && block->last == NULL;
}

struct branch_removed remove_branch_lox_ir_block(
        struct lox_ir_block * branch_block,
        bool true_branch,
        struct lox_allocator * lox_allocator
) {
    reset_loop_info(branch_block);

    struct lox_ir_block * branch_to_be_removed = true_branch ? branch_block->next_as.branch.true_branch : branch_block->next_as.branch.false_branch;
    struct lox_ir_block * branch_remains = true_branch ? branch_block->next_as.branch.false_branch : branch_block->next_as.branch.true_branch;
    if (branch_to_be_removed == NULL) {
        return (struct branch_removed) {
            .ssa_name_definitions_removed = empty_u64_set(lox_allocator),
            .blocks_removed = empty_u64_set(lox_allocator),
        };
    }

    struct u64_set blocks_removed = get_blocks_to_remove(branch_to_be_removed);

    //We get the set of ssa names removed
    struct u64_set_iterator subgraph_blocks_to_remove_it;
    init_u64_set_iterator(&subgraph_blocks_to_remove_it, blocks_removed);
    struct u64_set ssa_name_definitinos_subgraph;
    init_u64_set(&ssa_name_definitinos_subgraph, lox_allocator);
    while (has_next_u64_set_iterator(subgraph_blocks_to_remove_it)) {
        struct lox_ir_block * block_to_remove = (struct lox_ir_block *) next_u64_set_iterator(&subgraph_blocks_to_remove_it);
        union_u64_set(&ssa_name_definitinos_subgraph, block_to_remove->defined_ssa_names);
    }

    branch_block->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
    branch_block->next_as.next = branch_remains;
    remove_control_node_lox_ir_block(branch_block, branch_block->last); //The last is the jump_conditional control_node
    if(is_emtpy_ssa_block(branch_block)){
        add_u64_set(&blocks_removed, (uint64_t) branch_block);
        replace_block_lox_ir_block(branch_block, branch_remains);
    }

    return (struct branch_removed) {
        .ssa_name_definitions_removed = ssa_name_definitinos_subgraph,
        .blocks_removed = blocks_removed,
    };
}

//We get the set of blocks that we can remove, when we want to remove a branch. For example given the graph:
//Nodes: {A, B, C, D, E} Edges: A -> [B, C], B -> D, D -> E, C -> E
//If we concluide that we want to remove the branch A -> B. The final graph would be:
//Nodes: {A, C, E} Edges: A -> C, C -> E
static struct u64_set get_blocks_to_remove(struct lox_ir_block * start_block) {
    if(size_u64_set(start_block->predecesors) > 1){
        return empty_u64_set(NATIVE_LOX_ALLOCATOR());
    }

    struct u64_set blocks_to_be_removed;
    init_u64_set(&blocks_to_be_removed, NATIVE_LOX_ALLOCATOR());
    add_u64_set(&blocks_to_be_removed, (uint64_t) start_block);

    struct queue_list pending;
    init_queue_list(&pending, NATIVE_LOX_ALLOCATOR());

    enqueue_queue_list(&pending, start_block);

    while(!is_empty_queue_list(&pending)) {
        struct lox_ir_block * current_block = dequeue_queue_list(&pending);
        struct u64_set_iterator current_block_predecesors_it;
        init_u64_set_iterator(&current_block_predecesors_it, current_block->predecesors);

        //We check that the predecessors of the control_node that we are scanning, is contained the set of nodes that we want to
        //remove
        bool current_block_can_be_removed = true;
        while(has_next_u64_set_iterator(current_block_predecesors_it)){
            uint64_t current_block_predecesor_ptr = next_u64_set_iterator(&current_block_predecesors_it);
            struct lox_ir_block * current_block_predecesor = (struct lox_ir_block *) current_block_predecesor_ptr;

            if(!contains_u64_set(&blocks_to_be_removed, current_block_predecesor_ptr)){
                current_block_can_be_removed = false;
                break;
            }
        }

        if (current_block_can_be_removed) {
            add_u64_set(&blocks_to_be_removed, (uint64_t) current_block);

            if(current_block->type_next == TYPE_NEXT_LOX_IR_BLOCK_SEQ){
                enqueue_queue_list(&pending, current_block->next_as.next);
            } else if(current_block->type_next == TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
                enqueue_queue_list(&pending, current_block->next_as.branch.false_branch);
                enqueue_queue_list(&pending, current_block->next_as.branch.true_branch);
            }
        }
    }

    free_queue_list(&pending);

    return blocks_to_be_removed;
}

void replace_block_lox_ir_block(struct lox_ir_block * old_block, struct lox_ir_block * new_block) {
    reset_loop_info(old_block);
    reset_loop_info(new_block);

    FOR_EACH_U64_SET_VALUE(old_block->predecesors, predecesor_u64_ptr) {
        struct lox_ir_block * predecesor_node = (void *) predecesor_u64_ptr;
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

bool dominates_lox_ir_block(struct lox_ir_block * a, struct lox_ir_block * b, struct lox_allocator * allocator) {
    struct u64_set dominator_set_b = get_dominator_set_lox_ir_block(b, allocator);
    return contains_u64_set(&dominator_set_b, (uint64_t) a);
}

struct u64_set get_dominator_set_lox_ir_block(struct lox_ir_block * block, struct lox_allocator * allocator) {
    struct u64_set dominator_set;
    init_u64_set(&dominator_set, allocator);

    if (size_u64_set(block->predecesors) == 0) {
        add_u64_set(&dominator_set, (uint64_t) block);
    } else if(size_u64_set(block->predecesors) == 1) {
        struct lox_ir_block * predecessor = (struct lox_ir_block *) get_first_value_u64_set(block->predecesors);
        union_u64_set(&dominator_set, get_dominator_set_lox_ir_block(predecessor, allocator));
        add_u64_set(&dominator_set, (uint64_t) block);
    } else {
        int current_index = 0;

        FOR_EACH_U64_SET_VALUE(block->predecesors, predecesor_block_ptr) {
            struct lox_ir_block * predecessor = (struct lox_ir_block *) predecesor_block_ptr;
            struct u64_set dominator_set_predecessor = get_dominator_set_lox_ir_block(predecessor, allocator);

            if ((current_index++) == 0) {
                //Initialize
                union_u64_set(&dominator_set, dominator_set_predecessor);
            } else {
                intersection_u64_set(&dominator_set, dominator_set_predecessor);
            }
        }
    }

    return dominator_set;
}

static struct lox_ir_block_loop_info * create_loop_info_lox_ir_block(struct lox_ir_block * loop_condition_block, struct lox_allocator *allocator);

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
    struct lox_ir_block_loop_info * ssa_block_loop_info = extra;

    if(current_block->is_loop_condition){
        //Current block is the loop condtiion block that we want to scan. We want to keep scanning from it
        return true;
    }

    if (current_block->loop_condition_block != ssa_block_loop_info->condition_block) {
        //Dont keep scanning from current_block
        return false;
    }

    for(struct lox_ir_control_node * current_control_node = current_block->first;; current_control_node = current_control_node->next) {
        if(current_control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
            struct lox_ir_control_define_ssa_name_node * define_node = (struct lox_ir_control_define_ssa_name_node *) current_control_node;
            add_u8_set(&ssa_block_loop_info->modified_local_numbers, define_node->ssa_name.value.local_number);
        } else if(current_control_node->type == LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT){
            struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) current_control_node;
            struct lox_ir_data_node * array_instance = set_array_element->array;

            if (array_instance->type == LOX_IR_DATA_NODE_GET_SSA_NAME) {
                struct lox_ir_data_get_ssa_name_node * get_array_instance = (struct lox_ir_data_get_ssa_name_node *) array_instance;
                add_u8_set(&ssa_block_loop_info->modified_local_numbers, get_array_instance->ssa_name.value.local_number);
            }
        }

        if(current_control_node == current_block->last){
            break;
        }
    }

    return true;
}

static void reset_loop_info(struct lox_ir_block * block) {
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

static void record_removed_node_information_of_block(
        struct lox_ir_block * block,
        struct lox_ir_control_node * new_block_node
) {
    if(new_block_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME){
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) new_block_node;
        remove_u64_set(&block->defined_ssa_names, define->ssa_name.u16);
    }
}