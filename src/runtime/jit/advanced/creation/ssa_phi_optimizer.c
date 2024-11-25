#include "ssa_phi_optimizer.h"

struct optimize_phi_functions_consumer_struct {
    struct phi_insertion_result * phi_insertion_result;
    struct ssa_control_node * control_node;
    //u64_set of ssa_control_nodes per ssa name
    struct u64_hash_table * node_uses_by_ssa_name;
    struct ssa_block * block;
};

static void optimize_phi_functions_consumer (
        struct ssa_data_node * __,
        void ** parent_child_ptr,
        struct ssa_data_node * current_node,
        void * control_node_ptr
);
static void remove_innecesary_phi_function(struct ssa_data_phi_node * phi_node, void ** parent_child_ptr);
static void extract_phi_to_ssa_name(struct optimize_phi_functions_consumer_struct *, struct ssa_data_phi_node *, void ** parent_child_ptr);
static uint8_t allocate_new_ssa_version(int local_number, struct phi_insertion_result *);
static void add_ssa_name_uses_to_map(struct u64_hash_table * uses_by_ssa_node, struct ssa_control_node *);
static void add_ssa_name_use(struct u64_hash_table * uses_by_ssa_node, struct ssa_name, struct ssa_control_node *);

//Removes innecesary phi functions. Like a1 = phi(a0), it will replace it with: a1 = a0. a0 will be represented with the node
//Also extract phi nodes to ssa names, for example: print phi(a0, a1) -> a2 = phi(a0, a1); print a2
struct phi_optimization_result optimize_ssa_ir_phis(
        struct ssa_block * start_block,
        struct phi_insertion_result * phi_insertion_result
) {
    struct u64_hash_table uses_by_ssa_node;
    init_u64_hash_table(&uses_by_ssa_node, NATIVE_LOX_ALLOCATOR());
    struct stack_list pending;
    init_stack_list(&pending, NATIVE_LOX_ALLOCATOR());
    push_stack_list(&pending, start_block);

    while(!is_empty_stack_list(&pending)) {
        struct ssa_block * current_block = pop_stack_list(&pending);

        for(struct ssa_control_node * current = current_block->first;; current = current->next) {
            struct optimize_phi_functions_consumer_struct for_each_node_struct = (struct optimize_phi_functions_consumer_struct) {
                .node_uses_by_ssa_name = &uses_by_ssa_node,
                .phi_insertion_result = phi_insertion_result,
                .control_node = current,
                .block = current_block,
            };

            for_each_data_node_in_control_node(current, &for_each_node_struct, SSA_CONTROL_NODE_OPT_RECURSIVE, optimize_phi_functions_consumer);

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
            remove_innecesary_phi_function(phi_node, parent_child_ptr);
        } else if(size_u64_set(phi_node->ssa_versions) > 1 && for_each_node_consumer_struct->control_node->type != SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
            extract_phi_to_ssa_name(for_each_node_consumer_struct, (struct ssa_data_phi_node *) current_node, parent_child_ptr);
        }
    }

    add_ssa_name_uses_to_map(for_each_node_consumer_struct->node_uses_by_ssa_name, for_each_node_consumer_struct->control_node);
}

static void remove_innecesary_phi_function(
        struct ssa_data_phi_node * phi_node,
        void ** parent_child_ptr
) {
    uint8_t ssa_version = get_first_value_u64_set(phi_node->ssa_versions);

    struct ssa_data_get_ssa_name_node * new_get_ssa_name = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL
    );
    new_get_ssa_name->ssa_name = CREATE_SSA_NAME(phi_node->local_number, ssa_version);

    //Replace node
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
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node
    );
    extracted_define_ssa_name->ssa_name = extracted_ssa_name;
    extracted_define_ssa_name->value = &phi_node->data;

    struct ssa_data_get_ssa_name_node * get_extracted = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL
    );

    get_extracted->ssa_name = extracted_ssa_name;

    add_before_control_node_ssa_block(block, control_node, &extracted_define_ssa_name->control);
    *parent_child_ptr = get_extracted;
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

    for_each_data_node_in_control_node(control_node, &consumer_struct, SSA_CONTROL_NODE_OPT_RECURSIVE, add_ssa_name_uses_to_map_consumer);
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