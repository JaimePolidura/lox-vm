#include "lox_ir.h"

static struct u64_set get_blocks_to_remove(struct lox_ir_block*);
static void record_new_node_information_of_block(struct lox_ir*,struct lox_ir_block*,struct lox_ir_control_node*);
static void record_removed_node_information_of_block(struct lox_ir*,struct lox_ir_block*,struct lox_ir_control_node*);
static void add_v_register_definition_lox_ir(struct lox_ir*,int,struct lox_ir_control_node*);

void add_last_control_node_block_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * node
) {
    record_new_node_information_of_block(lox_ir, block, node);
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

void add_first_control_node_block_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * node
) {
    record_new_node_information_of_block(lox_ir, block, node);
    reset_loop_info(block);

    if(block->last == NULL){
        block->last = node;
    }
    if(block->first != NULL) {
        block->first->prev = node;
        node->next = block->first;
    }

    block->first = node;
}

void add_before_control_node_block_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * before,
        struct lox_ir_control_node * new
) {
    record_new_node_information_of_block(lox_ir, block, new);
    reset_loop_info(block);

    if (before == NULL) {
        add_last_control_node_block_lox_ir(lox_ir, block, new);
        return;
    }

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

void add_after_control_node_block_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * after,
        struct lox_ir_control_node * new
) {
    record_new_node_information_of_block(lox_ir, block, new);
    reset_loop_info(block);

    if(after == NULL){
        add_first_control_node_block_lox_ir(lox_ir, block, new);
        return;
    }

    if(block->first == NULL){
        block->first = new;
    }
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

void replace_control_node_block_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * prev,
        struct lox_ir_control_node * new
) {
    record_removed_node_information_of_block(lox_ir, block, prev);
    record_new_node_information_of_block(lox_ir, block, new);
    reset_loop_info(block);

    if (block->last == prev) {
        block->last = new;
    }
    if (block->first == prev) {
        block->first = new;
    }

    if (prev->prev != NULL) {
        prev->prev->next = new;
        new->prev = prev->prev;
    }
    if (prev->next != NULL) {
        prev->next->prev = new;
        new->next = prev->next;
    }
}

static void record_removed_node_information_of_block(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * removed_node
) {
    remove_u64_set(&lox_ir->exit_blocks, (uint64_t) removed_node);

    struct u64_set used_ssa_names = get_used_ssa_names_lox_ir_control(removed_node, NATIVE_LOX_ALLOCATOR());
    FOR_EACH_U64_SET_VALUE(used_ssa_names, uint64_t, used_ssa_name_u64) {
        struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_u64);
        remove_ssa_name_use_lox_ir(lox_ir, used_ssa_name, removed_node);
    }
    free_u64_set(&used_ssa_names);

    if (removed_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) removed_node;
        remove_u64_set(&block->defined_ssa_names, define->ssa_name.u16);
        remove_u64_hash_table(&lox_ir->cyclic_ssa_name_definitions, define->ssa_name.u16);
        remove_u64_hash_table(&lox_ir->node_uses_by_ssa_name, define->ssa_name.u16);
    }
}

static void record_new_node_information_of_block(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct lox_ir_control_node * new_block_node
) {
    struct u64_set used_ssa_names = get_used_ssa_names_lox_ir_control(new_block_node, NATIVE_LOX_ALLOCATOR());
    FOR_EACH_U64_SET_VALUE(used_ssa_names, uint64_t, used_ssa_name_u64) {
        struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_u64);
        add_ssa_name_use_lox_ir(lox_ir, used_ssa_name, new_block_node);
    }
    free_u64_set(&used_ssa_names);

    if (new_block_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) new_block_node;
        add_u64_set(&block->defined_ssa_names, define->ssa_name.u16);
        put_u64_hash_table(&lox_ir->definitions_by_ssa_name, define->ssa_name.u16, new_block_node);
    }
}

void remove_block_control_node_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * lox_ir_block,
        struct lox_ir_control_node * node_to_remove
) {
    record_removed_node_information_of_block(lox_ir, lox_ir_block, node_to_remove);
    reset_loop_info(lox_ir_block);

    //The block has only 1 control control_node_to_lower
    if(lox_ir_block->first == lox_ir_block->last){
        lox_ir_block->first = lox_ir_block->last = NULL;
        remove_only_block_lox_ir(lox_ir, lox_ir_block);
        return;
    }
    //We are removing the first control_node_to_lower
    if(lox_ir_block->first == node_to_remove) {
        lox_ir_block->first = node_to_remove->next;
    }
    //We are removing the last control_node_to_lower
    if(lox_ir_block->last == node_to_remove){
        lox_ir_block->last = node_to_remove->prev;
    }
    //There is only one control_node_to_lower remaining
    if(lox_ir_block->last == lox_ir_block->first){
        lox_ir_block->first->next = NULL;
        lox_ir_block->first->prev = NULL;
        return;
    }
    //Unlink the control_node_to_lower from the control control_node_to_lower linkedlist in a block
    if (node_to_remove->prev != NULL) {
        node_to_remove->prev->next = node_to_remove->next;
    }
    if(node_to_remove->next != NULL){
        node_to_remove->next->prev = node_to_remove->prev;
    }
}

void remove_only_block_lox_ir(struct lox_ir * lox_ir, struct lox_ir_block * block) {
    reset_loop_info(block);
    struct u64_set successors = get_successors_lox_ir_block(block, NATIVE_LOX_ALLOCATOR());
    struct u64_set predeccessors = block->predecesors;

    lox_assert_false(size_u64_set(successors) >= 1 && lox_ir->first_block == block, "lox_ir.c::remove_only_block_lox_ir",
                     "Cannot remove block when successors >= 1 and the first node on the IR is the node to be removed");
    lox_assert_false(size_u64_set(predeccessors) > 1 && size_u64_set(successors) > 1, "lox_ir.c::remove_only_block_lox_ir",
                     "Cannot remove block when predeccessors > 1 and successors > 1");
    lox_assert_false(size_u64_set(predeccessors) == 1 && size_u64_set(successors) > 1, "lox_ir.c::remove_only_block_lox_ir",
                     "Cannot remove block when predeccessors == 1 and successors > 1");
    lox_assert_false(size_u64_set(predeccessors) > 1 && size_u64_set(successors) == 1, "lox_ir.c::remove_only_block_lox_ir",
                     "Cannot remove block when predeccessors > 1 and successors == 1");
    lox_assert_false(size_u64_set(successors) == 0, "lox_ir.c::remove_only_block_lox_ir",
                     "Cannot remove block when successors == 0");

    FOR_EACH_U64_SET_VALUE(predeccessors, struct lox_ir_block *, predeccessor) {
        FOR_EACH_U64_SET_VALUE(successors, struct lox_ir_block *, successor) {
            if (predeccessor->type_next == TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
                if(predeccessor->next_as.branch.true_branch == block){
                    predeccessor->next_as.branch.true_branch = successor;
                } else {
                    predeccessor->next_as.branch.false_branch = successor;
                }
            } else {
                predeccessor->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
                predeccessor->next_as.next = successor;
            }

            remove_u64_set(&successor->predecesors, (uint64_t) block);
            add_u64_set(&successor->predecesors, (uint64_t) predeccessor);
        }
    }

    if(lox_ir->first_block == block){
        lox_ir->first_block = (struct lox_ir_block *) get_first_value_u64_set(successors);
    }
}

struct branch_removed remove_block_branch_lox_ir(
        struct lox_ir * lox_ir,
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

    //We get the set of jit names removed
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
    remove_block_control_node_lox_ir(lox_ir, branch_block, branch_block->last); //The last is the jump_conditional control_node_to_lower
    if(is_emtpy_lox_ir_block(branch_block)){
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

        //We check that the predecessors of the control_node_to_lower that we are scanning, is contained the set of nodes that we want to
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

struct u64_set get_all_block_paths_to_block_set_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * start_block,
        struct lox_ir_block * target_block,
        struct lox_allocator * allocator
) {
    struct u64_set block_paths;
    init_u64_set(&block_paths, allocator);
    add_u64_set(&block_paths, (uint64_t) start_block);

    struct stack_list pending;
    init_stack_list(&pending, allocator);
    push_stack_list(&pending, start_block);

    while (!is_empty_stack_list(&pending)) {
        struct lox_ir_block * current_block = pop_stack_list(&pending);

        FOR_EACH_U64_SET_VALUE(get_successors_lox_ir_block(current_block, allocator), struct lox_ir_block *, successor) {
            if (successor != target_block) {
                add_u64_set(&block_paths, (uint64_t) successor);
                push_stack_list(&pending, successor);
            }
        }
    }

    free_stack_list(&pending);

    return block_paths;
}

void insert_block_before_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * new_block,
        struct u64_set predecessors,
        struct lox_ir_block * successor
) {
    struct lox_ir_block * first_predecessor = (struct lox_ir_block *) get_first_value_u64_set(predecessors);

    //Initialize new_block
    new_block->nested_loop_body = first_predecessor != NULL ? first_predecessor->nested_loop_body : 0;
    new_block->is_loop_condition = false;
    new_block->loop_condition_block = first_predecessor != NULL ?
            (first_predecessor->loop_condition_block == first_predecessor ? first_predecessor : first_predecessor->loop_condition_block) :
            NULL;
    new_block->loop_info = NULL;
    new_block->predecesors = clone_u64_set(&predecessors, predecessors.inner_hash_table.allocator);

    if (new_block->type_next != TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
        new_block->type_next = successor != NULL ? TYPE_NEXT_LOX_IR_BLOCK_SEQ : TYPE_NEXT_LOX_IR_BLOCK_NONE;
        new_block->next_as.next = successor;
    }

    //Now we will "install" new_block to be the predecessors of the passed arg "successor"
    add_u64_set(&successor->predecesors, (uint64_t) new_block);

    FOR_EACH_U64_SET_VALUE(predecessors, struct lox_ir_block *, current_predecessor) {
        add_u64_set(&new_block->predecesors, (uint64_t) current_predecessor);
        remove_u64_set(&successor->predecesors, (uint64_t) current_predecessor);

        if (current_predecessor->type_next == TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
            if (current_predecessor->next_as.branch.true_branch == successor) {
                current_predecessor->next_as.branch.true_branch = new_block;
            } else {
                current_predecessor->next_as.branch.false_branch = new_block;
            }
        } else { // TYPE_NEXT_LOX_IR_BLOCK_SEQ
            current_predecessor->next_as.next = new_block;
        }
    }
}

bool dominates_block_lox_ir(struct lox_ir * lox_ir, struct lox_ir_block * a, struct lox_ir_block * b, struct lox_allocator * allocator) {
    struct u64_set dominator_set_b = get_block_dominator_set_lox_ir(lox_ir, b, allocator);
    return contains_u64_set(&dominator_set_b, (uint64_t) a);
}

struct u64_set get_block_dominator_set_lox_ir(struct lox_ir * lox_ir, struct lox_ir_block * block, struct lox_allocator * allocator) {
    struct u64_set dominator_set;
    init_u64_set(&dominator_set, allocator);

    if (size_u64_set(block->predecesors) == 0) {
        add_u64_set(&dominator_set, (uint64_t) block);
    } else if(size_u64_set(block->predecesors) == 1) {
        struct lox_ir_block * predecessor = (struct lox_ir_block *) get_first_value_u64_set(block->predecesors);
        union_u64_set(&dominator_set, get_block_dominator_set_lox_ir(lox_ir, predecessor, allocator));
        add_u64_set(&dominator_set, (uint64_t) block);
    } else {
        int current_index = 0;

        FOR_EACH_U64_SET_VALUE(block->predecesors, struct lox_ir_block *, predecessor) {
            struct u64_set dominator_set_predecessor = get_block_dominator_set_lox_ir(lox_ir, predecessor, allocator);

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

void remove_names_references_lox_ir(struct lox_ir * lox_ir, struct u64_set removed_ssa_names) {
    FOR_EACH_U64_SET_VALUE(removed_ssa_names, uint64_t, removed_ssa_name_u64) {
        struct ssa_name removed_ssa_name = CREATE_SSA_NAME_FROM_U64(removed_ssa_name_u64);
        remove_name_references_lox_ir(lox_ir, removed_ssa_name);
    }
}

void remove_name_references_lox_ir(struct lox_ir * lox_ir, struct ssa_name ssa_name_to_remove) {
    remove_u64_hash_table(&lox_ir->definitions_by_ssa_name, ssa_name_to_remove.u16);
    remove_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name_to_remove.u16);
}

struct ssa_name alloc_ssa_name_lox_ir(
        struct lox_ir * lox_ir,
        int ssa_version,
        char * local_name,
        struct lox_ir_block * block,
        struct lox_ir_type * type
) {
    int local_number = ++lox_ir->function->n_locals;
    struct ssa_name ssa_name = CREATE_SSA_NAME(local_number, ssa_version);
    local_name = dynamic_format_string("%s%i", local_name, local_number);
    put_u8_hash_table(&lox_ir->function->local_numbers_to_names, local_number, local_name);
    put_u8_hash_table(&lox_ir->max_version_allocated_per_local, local_number, (void *) ssa_version);

    struct u64_set * uses = alloc_u64_set(NATIVE_LOX_ALLOCATOR());
    put_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name.u16, uses);

    put_type_by_ssa_name_lox_ir(lox_ir, block, ssa_name, type);

    return ssa_name;
}

struct ssa_name alloc_ssa_version_lox_ir(struct lox_ir * lox_ir, int local_number) {
    uint8_t last_version = (uint8_t) get_u8_hash_table(&lox_ir->max_version_allocated_per_local, local_number);
    uint8_t new_version = last_version + 1;
    put_u8_hash_table(&lox_ir->max_version_allocated_per_local, local_number, (void *) new_version);
    return CREATE_SSA_NAME(local_number, new_version);
}

void remove_ssa_name_use_lox_ir(
    struct lox_ir * lox_ir,
    struct ssa_name ssa_name,
    struct lox_ir_control_node * control_node
) {
    if(contains_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name.u16)){
        struct u64_set * uses = get_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name.u16);
        remove_u64_set(uses, (uint64_t) control_node);
    }
}

void add_ssa_name_use_lox_ir(
        struct lox_ir * lox_ir,
        struct ssa_name ssa_name,
        struct lox_ir_control_node * control_node
) {
    if(!contains_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name.u16)){
        struct u64_set * uses = LOX_MALLOC(LOX_IR_ALLOCATOR(lox_ir), sizeof(struct u64_set));
        init_u64_set(uses, LOX_IR_ALLOCATOR(lox_ir));
        put_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name.u16, uses);
    }

    struct u64_set * uses = get_u64_hash_table(&lox_ir->node_uses_by_ssa_name, ssa_name.u16);
    add_u64_set(uses, (uint64_t) control_node);
}

struct lox_ir_type * get_type_by_ssa_name_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct ssa_name ssa_name
) {
    struct u64_hash_table * types_by_block = get_u64_hash_table(&lox_ir->type_by_ssa_name_by_block, (uint64_t) block);
    struct lox_ir_type * produced_type = get_u64_hash_table(types_by_block, ssa_name.u16);
    return produced_type;
}

void put_type_by_ssa_name_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct ssa_name ssa_name,
        struct lox_ir_type * new_type
) {
    if(new_type == NULL){
        return;
    }
    if(!contains_u64_hash_table(&lox_ir->type_by_ssa_name_by_block, (uint64_t) block)){
        struct u64_hash_table * types_by_block = LOX_MALLOC(LOX_IR_ALLOCATOR(lox_ir), sizeof(struct u64_hash_table));
        init_u64_hash_table(types_by_block, LOX_IR_ALLOCATOR(lox_ir));
        put_u64_hash_table(&lox_ir->type_by_ssa_name_by_block, (uint64_t) block, types_by_block);
    }

    struct u64_hash_table * types_by_block = get_u64_hash_table(&lox_ir->type_by_ssa_name_by_block, (uint64_t) block);
    put_u64_hash_table(types_by_block, ssa_name.u16, new_type);

    struct u64_hash_table_iterator type_by_ssa_name_by_block_ite;
    init_u64_hash_table_iterator(&type_by_ssa_name_by_block_ite, lox_ir->type_by_ssa_name_by_block);

    while (has_next_u64_hash_table_iterator(type_by_ssa_name_by_block_ite)) {
        struct u64_hash_table_entry current_entry = next_u64_hash_table_iterator(&type_by_ssa_name_by_block_ite);
        struct u64_hash_table * current_types_by_ssa_name = current_entry.value;

        if (contains_u64_hash_table(current_types_by_ssa_name, ssa_name.u16)) {
            put_u64_hash_table(current_types_by_ssa_name, ssa_name.u16, new_type);
        }
    }
}

static bool calculate_is_cyclic_definition(struct lox_ir * lox_ir, struct ssa_name target, struct ssa_name start);

//i2 = phi(i0, i1) i1 = i2; i2 = i1 + 1;
//is_cyclic_definition(parent = i2, name_to_check = i1) = true
bool is_cyclic_ssa_name_definition_lox_ir(struct lox_ir * lox_ir, struct ssa_name parent, struct ssa_name name_to_check) {
    uint64_t cyclic_name = parent.u16 << 16 | name_to_check.u16;
    if(contains_u64_hash_table(&lox_ir->cyclic_ssa_name_definitions, cyclic_name)){
        return (bool) get_u64_hash_table(&lox_ir->cyclic_ssa_name_definitions, cyclic_name);
    }

    bool is_cyclic = calculate_is_cyclic_definition(lox_ir, parent, name_to_check);
    put_u64_hash_table(&lox_ir->cyclic_ssa_name_definitions, cyclic_name, (void *) is_cyclic);

    return is_cyclic;
}

static bool calculate_is_cyclic_definition(struct lox_ir * lox_ir, struct ssa_name target, struct ssa_name start) {
    struct stack_list pending_evaluate;
    init_stack_list(&pending_evaluate, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending_evaluate, (void *) start.u16);

    struct u8_set visited_ssa_names;
    init_u8_set(&visited_ssa_names);

    while (!is_empty_stack_list(&pending_evaluate)) {
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64((uint64_t) pop_stack_list(&pending_evaluate));
        struct lox_ir_control_define_ssa_name_node * define_ssa_name = get_u64_hash_table(
                &lox_ir->definitions_by_ssa_name, current_ssa_name.u16);
        //If the definition is null, it means that the ssa name comes from a function arguemnt
        if (define_ssa_name == NULL) {
            continue;
        }

        struct u64_set used_ssa_names = get_used_ssa_names_lox_ir_data_node(define_ssa_name->value,
                NATIVE_LOX_ALLOCATOR());

        FOR_EACH_U64_SET_VALUE(used_ssa_names, uint64_t, used_ssa_name_u16) {
            struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_u16);

            if (used_ssa_name_u16 == target.u16) {
                return true;
            }
            if (contains_u8_set(&visited_ssa_names, used_ssa_name_u16)) {
                continue;
            }
            if (used_ssa_name.value.local_number == target.value.local_number) {
                push_stack_list(&pending_evaluate, (void *) used_ssa_name_u16);
                add_u8_set(&visited_ssa_names, used_ssa_name.value.version);
            }
        }

        free_u64_set(&used_ssa_names);
    }

    free_stack_list(&pending_evaluate);

    return false;
}

void on_ssa_name_def_moved_lox_ir(struct lox_ir * lox_ir, struct ssa_name name) {
    remove_u64_hash_table(&lox_ir->cyclic_ssa_name_definitions, name.u16);
}

struct lox_ir * alloc_lox_ir(struct lox_allocator * allocator, struct function_object * function, struct package * package) {
    struct lox_ir * lox_ir = LOX_MALLOC(allocator, sizeof(struct lox_ir));

    struct arena arena;
    init_arena(&arena);
    lox_ir->nodes_allocator_arena = alloc_lox_allocator_arena(arena, NATIVE_LOX_ALLOCATOR());

    init_u64_hash_table(&lox_ir->cyclic_ssa_name_definitions, LOX_IR_ALLOCATOR(lox_ir));
    init_u64_hash_table(&lox_ir->type_by_ssa_name_by_block, LOX_IR_ALLOCATOR(lox_ir));
    init_u64_hash_table(&lox_ir->definitions_by_ssa_name, LOX_IR_ALLOCATOR(lox_ir));
    init_u64_hash_table(&lox_ir->node_uses_by_ssa_name, LOX_IR_ALLOCATOR(lox_ir));
    init_u64_set(&lox_ir->exit_blocks, LOX_IR_ALLOCATOR(lox_ir));
    init_u8_hash_table(&lox_ir->max_version_allocated_per_local);

    lox_ir->function = function;
    lox_ir->package = package;
    lox_ir->first_block = NULL;

    return lox_ir;
}

void replace_ssa_name_lox_ir(struct lox_ir * lox_ir, struct ssa_name old, struct ssa_name new) {
    //Remove definition old
    struct lox_ir_control_node * definition_old_ssa_name = get_u64_hash_table(&lox_ir->definitions_by_ssa_name, old.u16);
    if (definition_old_ssa_name != NULL) {
        remove_block_control_node_lox_ir(lox_ir, definition_old_ssa_name->block, definition_old_ssa_name);
    }

    //Replace uses with new ssa name
    struct u64_set * uses_old_ssa_name = get_u64_hash_table(&lox_ir->node_uses_by_ssa_name, old.u16);
    FOR_EACH_U64_SET_VALUE(*uses_old_ssa_name, struct lox_ir_control_node *, use_node_old_ssa_name) {
        use_node_old_ssa_name.
    }
}
