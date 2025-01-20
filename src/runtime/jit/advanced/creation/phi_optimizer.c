#include "phi_optimizer.h"

struct optimize_phi_functions_consumer_struct {
    struct phi_insertion_result * phi_insertion_result;
    struct lox_ir_control_node * control_node;
    //u64_set of ssa_control_nodes per ssa name
    struct u64_hash_table * node_uses_by_ssa_name;
    struct lox_ir_block * block;
    struct arena_lox_allocator * nodes_allocator;
};

#define GET_NODES_ALLOCATOR(inserter) (&(inserter)->nodes_allocator->lox_allocator)

static bool optimize_phi_functions_consumer (
        struct lox_ir_data_node * __,
        void ** parent_child_ptr,
        struct lox_ir_data_node * current_node,
        void * control_node_ptr
);
static void remove_innecesary_phi_function(struct optimize_phi_functions_consumer_struct *, struct lox_ir_data_phi_node *, void ** parent_child_ptr);
static void extract_phi_to_ssa_name(struct optimize_phi_functions_consumer_struct *, struct lox_ir_data_phi_node *, void ** parent_child_ptr);
static uint8_t allocate_new_ssa_version(int local_number, struct phi_insertion_result *);
static void add_ssa_name_use(struct u64_hash_table * uses_by_ssa_node, struct ssa_name, struct lox_ir_control_node *);
static void propagate_extracted_phi(struct arena_lox_allocator*, struct lox_ir_block*, struct lox_ir_control_node*,
                                    struct lox_ir_data_phi_node*, uint8_t extracted_version);
static void fill_uses_by_node_hashtable(struct u64_hash_table *uses_by_ssa_node, struct lox_ir_block * start_block);
static bool fill_uses_by_node_hashtable_block(struct lox_ir_block *block, void *extra);

static bool optimize_lox_ir_phis_block(struct lox_ir_block *current_block, void * extra);

struct optimize_lox_ir_phis_block_struct {
    struct phi_insertion_result * phi_insertion_result;
    struct arena_lox_allocator * nodes_allocator;
    struct u64_hash_table * node_uses_by_ssa_name;
};

struct phi_optimization_result optimize_lox_ir_phis(
        struct lox_ir_block * start_block,
        struct phi_insertion_result * phi_insertion_result,
        struct arena_lox_allocator * nodes_allocator
) {
    struct u64_hash_table node_uses_by_ssa_name;
    init_u64_hash_table(&node_uses_by_ssa_name, &nodes_allocator->lox_allocator);

    struct optimize_lox_ir_phis_block_struct consumer_struct = (struct optimize_lox_ir_phis_block_struct) {
        .phi_insertion_result = phi_insertion_result,
        .nodes_allocator = nodes_allocator,
        .node_uses_by_ssa_name = &node_uses_by_ssa_name,
    };

    for_each_lox_ir_block(
            start_block,
            NATIVE_LOX_ALLOCATOR(),
            &consumer_struct,
            LOX_IR_BLOCK_OPT_REPEATED,
            &optimize_lox_ir_phis_block
    );

    fill_uses_by_node_hashtable(&node_uses_by_ssa_name, start_block);

    return (struct phi_optimization_result) {
        .node_uses_by_ssa_name = node_uses_by_ssa_name,
    };
}

static bool optimize_lox_ir_phis_block(struct lox_ir_block * current_block, void * extra) {
    struct optimize_lox_ir_phis_block_struct * consumer_struct = extra;

    for (struct lox_ir_control_node * current = current_block->first;; current = current->next) {
        struct optimize_phi_functions_consumer_struct for_each_node_struct = (struct optimize_phi_functions_consumer_struct) {
                .phi_insertion_result = consumer_struct->phi_insertion_result,
                .nodes_allocator = consumer_struct->nodes_allocator,
                .node_uses_by_ssa_name = consumer_struct->node_uses_by_ssa_name,
                .control_node = current,
                .block = current_block,
        };

        for_each_data_node_in_lox_ir_control(
                current,
                &for_each_node_struct,
                LOX_IR_DATA_NODE_OPT_POST_ORDER,
                &optimize_phi_functions_consumer
        );

        if(current == current_block->last){
            break;
        }
    }

    return true;
}

static bool optimize_phi_functions_consumer(
        struct lox_ir_data_node * parent,
        void ** parent_child_ptr,
        struct lox_ir_data_node * current_node,
        void * consumer_struct_ptr
) {
    struct optimize_phi_functions_consumer_struct * for_each_node_consumer_struct = consumer_struct_ptr;

    if (current_node->type == LOX_IR_DATA_NODE_PHI) {
        struct lox_ir_data_phi_node * phi_node = (struct lox_ir_data_phi_node *) current_node;
        if (size_u64_set(phi_node->ssa_versions) == 1) {
            remove_innecesary_phi_function(for_each_node_consumer_struct, phi_node, parent_child_ptr);
        } else if(size_u64_set(phi_node->ssa_versions) > 1) {
            if (parent == NULL && for_each_node_consumer_struct->control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
                return true;
            }

            extract_phi_to_ssa_name(for_each_node_consumer_struct, (struct lox_ir_data_phi_node *) current_node, parent_child_ptr);
        }
    }

    return true;
}

static void remove_innecesary_phi_function(
        struct optimize_phi_functions_consumer_struct * optimizer,
        struct lox_ir_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    uint8_t ssa_version = get_first_value_u64_set(phi_node->ssa_versions);

    struct lox_ir_data_get_ssa_name_node * new_get_ssa_name = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, GET_NODES_ALLOCATOR(optimizer)
    );
    new_get_ssa_name->ssa_name = CREATE_SSA_NAME(phi_node->local_number, ssa_version);

    //Replace control_node
    *parent_child_ptr = (void *) new_get_ssa_name;
}

//Extracts phi functions to ssa names
//Example: Given: print phi(a0, a1). Result: a2 = phi(a0, a1); print a2;
static void extract_phi_to_ssa_name(
        struct optimize_phi_functions_consumer_struct * consumer_struct,
        struct lox_ir_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    struct phi_insertion_result * phi_insertion_result = consumer_struct->phi_insertion_result;
    struct lox_ir_control_node * control_node = consumer_struct->control_node;
    struct lox_ir_block * block = consumer_struct->block;

    uint8_t extracted_version = allocate_new_ssa_version(phi_node->local_number, phi_insertion_result);
    struct ssa_name extracted_ssa_name = CREATE_SSA_NAME(phi_node->local_number, extracted_version);
    struct lox_ir_control_define_ssa_name_node * extracted_define_ssa_name = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME, struct lox_ir_control_define_ssa_name_node, block, GET_NODES_ALLOCATOR(consumer_struct)
    );
    extracted_define_ssa_name->ssa_name = extracted_ssa_name;
    extracted_define_ssa_name->value = &phi_node->data;
    put_u64_hash_table(&phi_insertion_result->ssa_definitions_by_ssa_name, extracted_ssa_name.u16, extracted_define_ssa_name);

    struct lox_ir_data_get_ssa_name_node * get_extracted = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, GET_NODES_ALLOCATOR(consumer_struct)
    );

    get_extracted->ssa_name = extracted_ssa_name;

    propagate_extracted_phi(consumer_struct->nodes_allocator, block, control_node, phi_node, extracted_version);

    add_before_control_node_lox_ir_block(block, control_node, &extracted_define_ssa_name->control);
    *parent_child_ptr = get_extracted;
}

//a2 = phi(a0, a1) Local number = "a". versions_extracted = {0, 1}. to_extracted_version = 2
struct propagation_extracted_phi {
    uint8_t local_number;
    struct u64_set versions_extracted;
    uint8_t to_extracted_version;
};

static bool propagate_extracted_phi_in_data_node(
        struct lox_ir_data_node * _,
        void ** __,
        struct lox_ir_data_node * current,
        void * extra
) {
    struct propagation_extracted_phi * propagation_extracted_phi = extra;

    if (current->type == LOX_IR_DATA_NODE_PHI &&
        ((struct lox_ir_data_phi_node *) current)->local_number == propagation_extracted_phi->local_number) {
        struct lox_ir_data_phi_node * current_phi_node = (struct lox_ir_data_phi_node *) current;

        if (is_subset_u64_set(current_phi_node->ssa_versions, propagation_extracted_phi->versions_extracted)) {
            //Remove propagation_extracted_phi->versions_extracted from current phi control_node
            difference_u64_set(&current_phi_node->ssa_versions, propagation_extracted_phi->versions_extracted);

            add_u64_set(&current_phi_node->ssa_versions, propagation_extracted_phi->to_extracted_version);
        }
    }

    return true;
}

static void propagate_extracted_phi_in_block(
        struct lox_ir_control_node * control_node,
        struct propagation_extracted_phi propagation_extracted_phi
){
    struct lox_ir_control_node * current_node = control_node;
    while (current_node != NULL) {
        for_each_data_node_in_lox_ir_control(
                current_node,
                &propagation_extracted_phi,
                LOX_IR_DATA_NODE_OPT_POST_ORDER,
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
        struct lox_ir_block * block,
        struct lox_ir_control_node * control_node,
        struct lox_ir_data_phi_node * phi_node,
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
    if(block->type_next == TYPE_NEXT_LOX_IR_BLOCK_SEQ){
        enqueue_queue_list(&pending, block->next_as.next);
    } else if(block->type_next == TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
        enqueue_queue_list(&pending, block->next_as.branch.false_branch);
        enqueue_queue_list(&pending, block->next_as.branch.true_branch);
    }

    while(!is_empty_queue_list(&pending)){
        struct lox_ir_block * current_block = dequeue_queue_list(&pending);
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

            if(current_block->type_next == TYPE_NEXT_LOX_IR_BLOCK_SEQ){
                enqueue_queue_list(&pending, current_block->next_as.next);
            } else if(current_block->type_next == TYPE_NEXT_LOX_IR_BLOCK_BRANCH) {
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

static void fill_uses_by_node_hashtable(
        struct u64_hash_table * uses_by_ssa_node,
        struct lox_ir_block * start_block
) {
    for_each_lox_ir_block(
            start_block,
            NATIVE_LOX_ALLOCATOR(),
            uses_by_ssa_node,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            fill_uses_by_node_hashtable_block
    );
}

static bool fill_uses_by_node_hashtable_block(struct lox_ir_block * block, void * extra) {
    struct u64_hash_table * uses_by_ssa_node = extra;
    for(struct lox_ir_control_node * current = block->first;; current = current->next){
        struct u64_set used_ssa_names = get_used_ssa_names_lox_ir_control(current, NATIVE_LOX_ALLOCATOR());

        FOR_EACH_U64_SET_VALUE(used_ssa_names, current_used_ssa_name_in_control_node_u64) {
            struct ssa_name current_used_ssa_name_in_control_node = CREATE_SSA_NAME_FROM_U64(current_used_ssa_name_in_control_node_u64);

            add_ssa_name_use(uses_by_ssa_node, current_used_ssa_name_in_control_node, current);
        }

        if(current == block->last){
            break;
        }
    }

    return true;
}

static void add_ssa_name_use(
        struct u64_hash_table * uses_by_ssa_node,
        struct ssa_name ssa_name,
        struct lox_ir_control_node * control_node
) {
    if(!contains_u64_hash_table(uses_by_ssa_node, ssa_name.u16)) {
        struct u64_set * u64_set = alloc_u64_set(NATIVE_LOX_ALLOCATOR());
        put_u64_hash_table(uses_by_ssa_node, ssa_name.u16, u64_set);
    }

    struct u64_set * uses = get_u64_hash_table(uses_by_ssa_node, ssa_name.u16);
    add_u64_set(uses, (uint64_t) control_node);
}