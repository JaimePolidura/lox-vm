#include "lox_ir.h"

static void link_predecessors_to_block(struct u64_set, struct lox_ir_block *);
static struct u64_set get_blocks_to_remove(struct lox_ir_block*);

void remove_block_control_node_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * lox_ir_block,
        struct lox_ir_control_node * node_to_remove
) {
    record_removed_node_information_of_block(lox_ir_block, node_to_remove);
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

    if(size_u64_set(successors) >= 1 && lox_ir->first_block == block){
        //TODO Runtime error
    }
    if (size_u64_set(predeccessors) >= 1 && size_u64_set(successors) >= 1) {
        //TODO Runtime error
    }
    if (size_u64_set(successors) == 0) {
        //TODO Runtime error
    }

    FOR_EACH_U64_SET_VALUE(predeccessors, predeccessor_ptr_u64) {
        struct lox_ir_block * predeccessor = (struct lox_ir_block *) predeccessor_ptr_u64;

        FOR_EACH_U64_SET_VALUE(successors, successor_ptr_u64) {
            struct lox_ir_block * successor = (struct lox_ir_block *) successor_ptr_u64;

            predeccessor->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
            predeccessor->next_as.next = successor;

            clear_u64_set(&successor->predecesors);
            add_u64_set(&successor->predecesors, (uint64_t) predeccessor);
        }
    }

    if(lox_ir->first_block == block){
        lox_ir->first_block = (struct lox_ir_block *) get_first_value_u64_set(successors);
    }

    clear_u64_set(&successors);
}

//We are going to link predeccessors nodes to block
static void link_predecessors_to_block(struct u64_set predeccessors, struct lox_ir_block * block) {
    FOR_EACH_U64_SET_VALUE(predeccessors, predeccessor_ptr_u64) {
        struct lox_ir_block * predeccessor = (struct lox_ir_block *) predeccessor_ptr_u64;

        predeccessor->type_next = TYPE_NEXT_LOX_IR_BLOCK_SEQ;
        predeccessor->next_as.next = block;
        add_u64_set(&block->predecesors, (uint64_t) predeccessor);
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

        FOR_EACH_U64_SET_VALUE(block->predecesors, predecesor_block_ptr) {
            struct lox_ir_block * predecessor = (struct lox_ir_block *) predecesor_block_ptr;
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
    FOR_EACH_U64_SET_VALUE(removed_ssa_names, removed_ssa_name_u64) {
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

void add_v_register_use_lox_ir(
        struct lox_ir * lox_ir,
        int v_reg,
        struct lox_ir_control_node * control_node
) {
    if(!contains_u64_hash_table(&lox_ir->node_uses_by_v_reg, v_reg)){
        struct u64_set * uses = LOX_MALLOC(LOX_IR_ALLOCATOR(lox_ir), sizeof(struct u64_set));
        init_u64_set(uses, LOX_IR_ALLOCATOR(lox_ir));
        put_u64_hash_table(&lox_ir->node_uses_by_v_reg, v_reg, uses);
    }

    struct u64_set * uses = get_u64_hash_table(&lox_ir->node_uses_by_v_reg, v_reg);
    add_u64_set(uses, (uint64_t) control_node);
}

void add_v_register_definition_lox_ir(
        struct lox_ir * lox_ir,
        int v_reg,
        struct lox_ir_control_node * control
) {
    if(!contains_u64_hash_table(&lox_ir->definitions_by_v_reg, v_reg)){
        struct u64_set * uses = LOX_MALLOC(LOX_IR_ALLOCATOR(lox_ir), sizeof(struct u64_set));
        init_u64_set(uses, LOX_IR_ALLOCATOR(lox_ir));
        put_u64_hash_table(&lox_ir->definitions_by_v_reg, v_reg, uses);
    }

    struct u64_set * uses = get_u64_hash_table(&lox_ir->definitions_by_v_reg, v_reg);
    add_u64_set(uses, (uint64_t) control);
}

struct lox_ir_type * get_type_by_ssa_name_lox_ir(
        struct lox_ir * lox_ir,
        struct lox_ir_block * block,
        struct ssa_name ssa_name
) {
    struct u64_hash_table * types_by_block = get_u64_hash_table(&lox_ir->type_by_ssa_name_by_block, (uint64_t) block);
    return get_u64_hash_table(types_by_block, ssa_name.u16);
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

struct v_register alloc_v_register_lox_ir(struct lox_ir * lox_ir, bool is_float_register) {
    return ((struct v_register) {
        .number = lox_ir->last_v_reg_allocated++,
        .is_float_register = is_float_register,
        .register_bit_size = 64,
    });
}