#include "ssa_phi_optimizer.h"

struct optimize_phi_functions_consumer_struct {
    struct phi_insertion_result * phi_insertion_result;
    struct ssa_control_node * control_node;
    //u64_set of ssa_control_nodes per ssa name
    struct u64_hash_table * node_uses_by_ssa_name;
    struct ssa_block * block;
    struct arena_lox_allocator * ssa_nodes_allocator;
};

#define GET_LOX_ALLOCATOR(inserter) (&(inserter)->ssa_nodes_allocator->lox_allocator)

static void optimize_phi_functions_consumer (
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * control_node_ptr
);
static void remove_innecesary_phi_function(struct optimize_phi_functions_consumer_struct *, struct ssa_data_phi_node *, void ** parent_child_ptr);
static void extract_phi_to_ssa_name(struct optimize_phi_functions_consumer_struct *, struct ssa_data_phi_node *, void ** parent_child_ptr);
static uint8_t allocate_new_ssa_version(int local_number, struct phi_insertion_result *);
static void add_ssa_name_uses_to_map(struct u64_hash_table * uses_by_ssa_node, struct ssa_control_node *);
static void add_ssa_name_use(struct u64_hash_table * uses_by_ssa_node, struct ssa_name, struct ssa_control_node *);
static void propagate_extracted_phi(struct arena_lox_allocator*, struct ssa_block*, struct ssa_control_node*,
        struct ssa_data_phi_node*, uint8_t extracted_version);

struct phi_optimization_result optimize_ssa_ir_phis(
        struct ssa_block * start_block,
        struct phi_insertion_result * phi_insertion_result,
        struct arena_lox_allocator * ssa_nodes_allocator
) {
    struct u64_hash_table uses_by_ssa_node;
    init_u64_hash_table(&uses_by_ssa_node, &ssa_nodes_allocator->lox_allocator);
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, start_block);

    while(!is_empty_stack_list(&pending)) {
        struct ssa_block * current_block = pop_stack_list(&pending);

        for(struct ssa_control_node * current = current_block->first;; current = current->next) {
            struct optimize_phi_functions_consumer_struct for_each_node_struct = (struct optimize_phi_functions_consumer_struct) {
                .phi_insertion_result = phi_insertion_result,
                .ssa_nodes_allocator = ssa_nodes_allocator,
                .node_uses_by_ssa_name = &uses_by_ssa_node,
                .control_node = current,
                .block = current_block,
            };

            for_each_data_node_in_control_node(
                    current,
                    &for_each_node_struct,
                    SSA_DATA_NODE_OPT_POST_ORDER | SSA_DATA_NODE_OPT_RECURSIVE,
                    optimize_phi_functions_consumer
            );

            if(current == current_block->last){
                break;
            }
        }

        switch(current_block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_stack_list(&pending, current_block->next_as.next);
                break;

            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_stack_list(&pending, current_block->next_as.branch.false_branch);
                push_stack_list(&pending, current_block->next_as.branch.true_branch);
                break;

            case TYPE_NEXT_SSA_BLOCK_LOOP:
            case TYPE_NEXT_SSA_BLOCK_NONE:
                break;
        }
    }

    free_stack_list(&pending);

    return (struct phi_optimization_result) {
        .node_uses_by_ssa_name = uses_by_ssa_node,
    };
}

static void optimize_phi_functions_consumer(
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * consumer_struct_ptr
) {
    struct optimize_phi_functions_consumer_struct * for_each_node_consumer_struct = consumer_struct_ptr;

    if (current_node->type == SSA_DATA_NODE_TYPE_PHI) {
        struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current_node;
        if (size_u64_set(phi_node->ssa_versions) == 1) {
            remove_innecesary_phi_function(for_each_node_consumer_struct, phi_node, parent_child_ptr);
        } else if(size_u64_set(phi_node->ssa_versions) > 1 && for_each_node_consumer_struct->control_node->type != SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
            extract_phi_to_ssa_name(for_each_node_consumer_struct, (struct ssa_data_phi_node *) current_node, parent_child_ptr);
        }
    }

    add_ssa_name_uses_to_map(for_each_node_consumer_struct->node_uses_by_ssa_name, for_each_node_consumer_struct->control_node);
}

static void remove_innecesary_phi_function(
        struct optimize_phi_functions_consumer_struct * optimizer,
        struct ssa_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    uint8_t ssa_version = get_first_value_u64_set(phi_node->ssa_versions);

    struct ssa_data_get_ssa_name_node * new_get_ssa_name = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, GET_LOX_ALLOCATOR(optimizer)
    );
    new_get_ssa_name->ssa_name = CREATE_SSA_NAME(phi_node->local_number, ssa_version);

    //Replace control_node
    *parent_child_ptr = (void *) new_get_ssa_name;
}

//Extracts phi functions to ssa names
//Example: Given: print phi(a0, a1). Result: a2 = phi(a0, a1); print a2;
static void extract_phi_to_ssa_name(
        struct optimize_phi_functions_consumer_struct * consumer_struct,
        struct ssa_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    struct phi_insertion_result * phi_insertion_result = consumer_struct->phi_insertion_result;
    struct ssa_control_node * control_node = consumer_struct->control_node;
    struct ssa_block * block = consumer_struct->block;

    uint8_t extracted_version = allocate_new_ssa_version(phi_node->local_number, phi_insertion_result);
    struct ssa_name extracted_ssa_name = CREATE_SSA_NAME(phi_node->local_number, extracted_version);
    struct ssa_control_define_ssa_name_node * extracted_define_ssa_name = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node, block, GET_LOX_ALLOCATOR(consumer_struct)
    );
    extracted_define_ssa_name->ssa_name = extracted_ssa_name;
    extracted_define_ssa_name->value = &phi_node->data;
    put_u64_hash_table(&phi_insertion_result->ssa_definitions_by_ssa_name, extracted_ssa_name.u16, extracted_define_ssa_name);

    struct ssa_data_get_ssa_name_node * get_extracted = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, GET_LOX_ALLOCATOR(consumer_struct)
    );

    get_extracted->ssa_name = extracted_ssa_name;

    propagate_extracted_phi(consumer_struct->ssa_nodes_allocator, block, control_node, phi_node, extracted_version);

    add_before_control_node_ssa_block(block, control_node, &extracted_define_ssa_name->control);
    *parent_child_ptr = get_extracted;
}

//a2 = phi(a0, a1) Local number = "a". versions_extracted = {0, 1}. to_extracted_version = 2
struct propagation_extracted_phi {
    uint8_t local_number;
    struct u64_set versions_extracted;
    uint8_t to_extracted_version;
};

static void propagate_extracted_phi_in_data_node(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * current,
        void * extra
) {
    struct propagation_extracted_phi * propagation_extracted_phi = extra;

    if (current->type == SSA_DATA_NODE_TYPE_PHI &&
        ((struct ssa_data_phi_node *) current)->local_number == propagation_extracted_phi->local_number) {
        struct ssa_data_phi_node * current_phi_node = (struct ssa_data_phi_node *) current;

        if (is_subset_u64_set(current_phi_node->ssa_versions, propagation_extracted_phi->versions_extracted)) {
            //Remove propagation_extracted_phi->versions_extracted from current phi control_node
            difference_u64_set(&current_phi_node->ssa_versions, propagation_extracted_phi->versions_extracted);

            add_u64_set(&current_phi_node->ssa_versions, propagation_extracted_phi->to_extracted_version);
        }
    }
}

static void propagate_extracted_phi_in_block(
        struct ssa_control_node * control_node,
        struct propagation_extracted_phi propagation_extracted_phi
){
    struct ssa_control_node * current_node = control_node;
    while (current_node != NULL) {
        for_each_data_node_in_control_node(
                current_node,
                &propagation_extracted_phi,
                SSA_DATA_NODE_OPT_POST_ORDER | SSA_DATA_NODE_OPT_RECURSIVE,
                propagate_extracted_phi_in_data_node
        );

        current_node = current_node->next;
    }
}

//Given block1: print phi(a1, a2) and block 2: return phi(a1, a2)
//When print phi(a1, a2) is extracted by the method extract_phi_to_ssa_name(), it will generate: a3 = phi(a1, a2); print a3
//This method will propagate a3 to replace phi nodes of nodes a1 and a2. In the example:
//a3 = phi(a1, a2); print a3; return a3.
static void propagate_extracted_phi(
        struct arena_lox_allocator * arena_lox_allocator,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct ssa_data_phi_node * phi_node,
        uint8_t extracted_version
) {
    struct propagation_extracted_phi propagation_extracted_phi = {
            .versions_extracted = phi_node->ssa_versions,
            .local_number = phi_node->local_number,
            .to_extracted_version = extracted_version,
    };

    //Propagate change in the block where the extracted phi was done
    propagate_extracted_phi_in_block(control_node->next, propagation_extracted_phi);

    //Propagate the change to all subblocks of the subgraph whose only parent is block
    struct queue_list pending;
    init_queue_list(&pending, &arena_lox_allocator->lox_allocator);
    enqueue_queue_list(&pending, block);
    struct u64_set expeced_predecessors;
    init_u64_set(&expeced_predecessors, &arena_lox_allocator->lox_allocator);
    add_u64_set(&expeced_predecessors, (uint64_t) block);
    if(block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_SEQ){
        enqueue_queue_list(&pending, block->next_as.next);
    } else if(block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_BRANCH) {
        enqueue_queue_list(&pending, block->next_as.branch.false_branch);
        enqueue_queue_list(&pending, block->next_as.branch.true_branch);
    }

    while(!is_empty_queue_list(&pending)){
        struct ssa_block * current_block = dequeue_queue_list(&pending);
        bool can_extracted_phi_be_propagated = true;

        FOR_EACH_U64_SET_VALUE(current_block->predecesors, current_predecesor) {
            if(!contains_u64_set(&expeced_predecessors, current_predecesor)){
                can_extracted_phi_be_propagated = false;
                break;
            }
        }

        if(can_extracted_phi_be_propagated){
            add_u64_set(&expeced_predecessors, (uint64_t) current_block);
            propagate_extracted_phi_in_block(current_block->first, propagation_extracted_phi);

            if(current_block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_SEQ){
                enqueue_queue_list(&pending, current_block->next_as.next);
            } else if(current_block->type_next_ssa_block == TYPE_NEXT_SSA_BLOCK_BRANCH) {
                enqueue_queue_list(&pending, current_block->next_as.branch.false_branch);
                enqueue_queue_list(&pending, current_block->next_as.branch.true_branch);
            }
        }
    }
}

static uint8_t allocate_new_ssa_version(int local_number, struct phi_insertion_result * phi_insertion_result) {
    uint8_t last_version = (uint8_t) get_u8_hash_table(&phi_insertion_result->max_version_allocated_per_local, local_number);
    uint8_t new_version = last_version + 1;
    put_u8_hash_table(&phi_insertion_result->max_version_allocated_per_local, local_number, (void *) new_version);

    return new_version;
}

struct add_ssa_name_uses_to_map_consumer_struct {
    struct u64_hash_table * uses_by_ssa_node;
    struct ssa_control_node * control_node;
};

static void add_ssa_name_uses_to_map_consumer(
        struct ssa_data_node * __,
        void ** _,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct add_ssa_name_uses_to_map_consumer_struct * consumer_struct = extra;

    if (current_node->type == SSA_DATA_NODE_TYPE_PHI) {
        FOR_EACH_VERSION_IN_PHI_NODE((struct ssa_data_phi_node *) current_node, ssa_name) {
            add_ssa_name_use(consumer_struct->uses_by_ssa_node, ssa_name, consumer_struct->control_node);
        }
    } else if(current_node->type == SSA_DATA_NODE_TYPE_GET_SSA_NAME) {
        struct ssa_data_get_ssa_name_node * get_name = (struct ssa_data_get_ssa_name_node *) current_node;
        add_ssa_name_use(consumer_struct->uses_by_ssa_node, get_name->ssa_name, consumer_struct->control_node);
    }
}

static void add_ssa_name_uses_to_map(
        struct u64_hash_table * uses_by_ssa_node,
        struct ssa_control_node * control_node
) {
    struct add_ssa_name_uses_to_map_consumer_struct consumer_struct = (struct add_ssa_name_uses_to_map_consumer_struct) {
        .uses_by_ssa_node = uses_by_ssa_node,
        .control_node = control_node,
    };

    for_each_data_node_in_control_node(
            control_node,
            &consumer_struct,
            SSA_DATA_NODE_OPT_POST_ORDER | SSA_DATA_NODE_OPT_RECURSIVE,
            add_ssa_name_uses_to_map_consumer
    );
}

static void add_ssa_name_use(
        struct u64_hash_table * uses_by_ssa_node,
        struct ssa_name ssa_name,
        struct ssa_control_node * control_node
) {
    if(!contains_u64_hash_table(uses_by_ssa_node, ssa_name.u16)) {
        struct u64_set * u64_set = alloc_u64_set(NATIVE_LOX_ALLOCATOR());
        put_u64_hash_table(uses_by_ssa_node, ssa_name.u16, u64_set);
    }

    struct u64_set * uses = get_u64_hash_table(uses_by_ssa_node, ssa_name.u16);
    add_u64_set(uses, (uint64_t) control_node);
}