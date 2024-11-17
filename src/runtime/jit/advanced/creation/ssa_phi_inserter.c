#include "runtime/jit/advanced/ssa_block.h"

#include "shared/utils/collections/u64_hash_table.h"
#include "shared/utils/collections/u8_hash_table.h"
#include "shared/utils/collections/stack_list.h"
#include "shared/utils/collections/u64_set.h"

struct ssa_phi_inserter {
    struct u8_hash_table max_version_allocated_per_local;
    struct u64_set loops_evaluted; //Stores ssa_block->next.loop address
    struct u64_set extracted_variables_loop_body;
    struct stack_list pending_evaluate;
    //Key struct ssa_name, Value: pointer to ssa_control_node
    struct u64_hash_table ssa_definitions_by_ssa_name;
};

struct pending_evaluate {
    struct ssa_block * pending_block_to_evaluate;
    struct u8_hash_table * parent_versions;
};

static void push_pending_evaluate(struct ssa_phi_inserter *, struct ssa_block *, struct u8_hash_table * parent_versions);
static void insert_phis_in_block(struct ssa_phi_inserter *, struct ssa_block *block, struct u8_hash_table * parent_versions);
static void insert_ssa_versions_in_control_node(struct ssa_phi_inserter *, struct ssa_block *, struct ssa_control_node * control_node, struct u8_hash_table * parent_versions);
static uint8_t allocate_new_version(struct u8_hash_table * max_version_allocated_per_local, uint8_t local_number);
static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number);
static struct ssa_phi_inserter * alloc_ssa_phi_inserter();
static void free_ssa_phi_inserter(struct ssa_phi_inserter *);
static void extract_get_local(struct ssa_phi_inserter *inserter, struct u8_hash_table *parent_versions,
                              struct ssa_control_node *control_node_to_extract, struct ssa_block *,
                              uint8_t local_number);
static void insert_phis_in_data_node_consumer(
        struct ssa_data_node * parent,
        void ** parent_current_ptr,
        struct ssa_data_node * current_node,
        void * extra
);

void insert_ssa_ir_phis(
        struct ssa_block * start_block
) {
    start_block = start_block->next.next;

    struct ssa_phi_inserter * inserter = alloc_ssa_phi_inserter();

    push_pending_evaluate(inserter, start_block, alloc_u8_hash_table());

    while(!is_empty_stack_list(&inserter->pending_evaluate)) {
        struct pending_evaluate * pending_evaluate = pop_stack_list(&inserter->pending_evaluate);
        struct ssa_block * block_to_evaluate = pending_evaluate->pending_block_to_evaluate;
        struct u8_hash_table * parent_versions = pending_evaluate->parent_versions;
        free(pending_evaluate);

        insert_phis_in_block(inserter, block_to_evaluate, parent_versions);

        switch (block_to_evaluate->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ:
                push_pending_evaluate(inserter, block_to_evaluate->next.next, parent_versions);
                break;
            case TYPE_NEXT_SSA_BLOCK_LOOP:
                struct ssa_block * to_jump_loop_block = block_to_evaluate->next.loop;
                if(!contains_u64_set(&inserter->loops_evaluted, (uint64_t) to_jump_loop_block)){
                    push_pending_evaluate(inserter, to_jump_loop_block, parent_versions);
                    add_u64_set(&inserter->loops_evaluted, (uint64_t) to_jump_loop_block);
                } else {
                    //OP_LOOP always points to the loop condition
                    //If an assigment have ocurred inside the loop body, we will need to propagate outside the loop
                    push_pending_evaluate(inserter, to_jump_loop_block->next.branch.false_branch, parent_versions);
                }
                break;
            case TYPE_NEXT_SSA_BLOCK_BRANCH:
                push_pending_evaluate(inserter, block_to_evaluate->next.branch.false_branch, clone_u8_hash_table(parent_versions));
                push_pending_evaluate(inserter, block_to_evaluate->next.branch.true_branch, parent_versions);
                break;
            case TYPE_NEXT_SSA_BLOCK_NONE:
                free(parent_versions);
                break;
        }
    }

    free_ssa_phi_inserter(inserter);
}

static void insert_phis_in_block(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * block,
        struct u8_hash_table * parent_versions
) {
    for(struct ssa_control_node * current = block->first;; current = current->next.next){
        insert_ssa_versions_in_control_node(inserter, block, current, parent_versions);

        if(current == block->last){
            break;
        }
    }
}

struct insert_phis_in_data_node_struct {
    struct ssa_control_node * control_node;
    struct u8_hash_table * parent_versions;
    struct ssa_phi_inserter * inserter;
    struct ssa_block * block;
};

static void insert_ssa_versions_in_control_node(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct u8_hash_table * parent_versions
) {
    struct insert_phis_in_data_node_struct consumer_struct = (struct insert_phis_in_data_node_struct) {
        .control_node = control_node,
        .parent_versions = parent_versions,
        .inserter = inserter,
        .block = block,
    };

    for_each_data_node_in_control_node(control_node, &consumer_struct, &insert_phis_in_data_node_consumer);

    if (control_node->type == SSA_CONTORL_NODE_TYPE_SET_LOCAL) {
        //set_local node and define_ssa_name have the same memory outlay, we do this, so we can change the node easily in the graph
        //without creating any new node
        struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) control_node;
        struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) control_node;
        uint8_t local_number = (uint8_t) set_local->local_number;

        uint8_t new_version = allocate_new_version(&inserter->max_version_allocated_per_local, local_number);
        define_ssa_name->control.type = SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME;
        struct ssa_name ssa_name = CREATE_SSA_NAME(local_number, new_version);
        define_ssa_name->ssa_name = ssa_name;

        put_u64_hash_table(&inserter->ssa_definitions_by_ssa_name, ssa_name.u16, define_ssa_name);
        put_u8_hash_table(parent_versions, local_number, (void *) new_version);
    }
}

static void insert_phis_in_data_node_consumer(
        struct ssa_data_node * parent,
        void ** parent_current_ptr,
        struct ssa_data_node * current_node,
        void * extra
) {
    struct insert_phis_in_data_node_struct * consumer_struct = extra;
    struct u8_hash_table * parent_versions = consumer_struct->parent_versions;
    struct ssa_control_node * control_node = consumer_struct->control_node;
    struct ssa_phi_inserter * inserter = consumer_struct->inserter;
    struct ssa_block * block = consumer_struct->block;

    //We replace nodes with type SSA_DATA_NODE_TYPE_GET_LOCAL to SSA_DATA_NODE_TYPE_PHI
    if (current_node->type == SSA_DATA_NODE_TYPE_GET_LOCAL) {
        //We don't want to extract a = b; we want to extract only get_locals that are inside an expression
        bool inside_expression = parent != NULL;
        
        struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) current_node;
        uint8_t local_number = get_local->local_number;

        if(block->loop_body &&
           inside_expression &&
           contains_u8_set(&block->use_before_assigment, local_number) &&
           !contains_u64_set(&inserter->extracted_variables_loop_body, (uint64_t) control_node)
        ){
            extract_get_local(inserter, parent_versions, control_node, block, local_number);
        } else {
            //We are going to replace the OP_GET_LOCAL node with a phi node
            struct ssa_name ssa_name = CREATE_SSA_NAME(get_local->local_number, get_version(parent_versions, get_local->local_number));
            struct ssa_data_phi_node * phi_node = ALLOC_SSA_DATA_NODE(
                    SSA_DATA_NODE_TYPE_PHI, struct ssa_data_phi_node, current_node->original_bytecode
            );
            phi_node->local_number = local_number;
            init_u64_set(&phi_node->ssa_definitions);
            struct ssa_control_node * ssa_definition_node = get_u64_hash_table(&inserter->ssa_definitions_by_ssa_name, ssa_name.u16);
            add_u64_set(&phi_node->ssa_definitions, (uint64_t) ssa_definition_node);

            //Replace parent pointer to child node
            //OP_GET_LOCAL will always be a child of another data node.
            *parent_current_ptr = &phi_node->data;
        }
    } else if (current_node->type == SSA_DATA_NODE_TYPE_PHI) {
        struct ssa_data_phi_node * phi_node = (struct ssa_data_phi_node *) current_node;
        uint8_t last_version = get_version(parent_versions, phi_node->local_number);
        struct ssa_name new_ssa_name = CREATE_SSA_NAME(phi_node->local_number, last_version);
        struct ssa_control_node * last_definition = get_u64_hash_table(&inserter->ssa_definitions_by_ssa_name, new_ssa_name.u16);

        add_u64_set(&phi_node->ssa_definitions, (uint64_t) last_definition);
    }
}

static uint8_t allocate_new_version(struct u8_hash_table * max_version_allocated_per_local, uint8_t local_number) {
    uintptr_t new_version = 1;

    if(contains_u8_hash_table(max_version_allocated_per_local, local_number)){
        int old_version = (int) get_u8_hash_table(max_version_allocated_per_local, local_number);
        new_version = old_version + 1;
    }

    put_u8_hash_table(max_version_allocated_per_local, local_number, (void *) new_version);

    return new_version;
}

static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number) {
    if(contains_u8_hash_table(parent_versions, local_number)){
        return (int) get_u8_hash_table(parent_versions, local_number);
    } else {
        put_u8_hash_table(parent_versions, local_number, (void *) 0);
        return 0;
    }
}

static void push_pending_evaluate(
        struct ssa_phi_inserter * inserter,
        struct ssa_block * parent,
        struct u8_hash_table * parent_versions
) {
    struct pending_evaluate * pending_evaluate = malloc(sizeof(struct pending_evaluate));
    pending_evaluate->parent_versions = parent_versions;
    pending_evaluate->pending_block_to_evaluate = parent;
    push_stack_list(&inserter->pending_evaluate, pending_evaluate);
}

static struct ssa_phi_inserter * alloc_ssa_phi_inserter() {
    struct ssa_phi_inserter * inserter = malloc(sizeof(struct ssa_phi_inserter));
    init_u8_hash_table(&inserter->max_version_allocated_per_local);
    init_u64_hash_table(&inserter->ssa_definitions_by_ssa_name);
    init_u64_set(&inserter->extracted_variables_loop_body);
    init_stack_list(&inserter->pending_evaluate);
    init_u64_set(&inserter->loops_evaluted);
    return inserter;
}

static void free_ssa_phi_inserter(struct ssa_phi_inserter * inserter) {
    free_stack_list(&inserter->pending_evaluate);
    free_u64_set(&inserter->loops_evaluted);
    free(inserter);
}

//a = a + 1; will be replaced: a1 = a0; a2 = a1 + 1;
static void extract_get_local(
        struct ssa_phi_inserter * inserter,
        struct u8_hash_table *parent_versions,
        struct ssa_control_node *control_node_to_extract,
        struct ssa_block * to_extract_block,
        uint8_t local_number
) {
    //a0 in the example, will be a phi node
    struct ssa_name get_ssa_name = CREATE_SSA_NAME(local_number, get_version(parent_versions, local_number));
    struct ssa_data_phi_node * phi_node = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_PHI, struct ssa_data_phi_node, NULL);
    init_u64_set(&phi_node->ssa_definitions);
    add_u64_set(&phi_node->ssa_definitions, (uint64_t) get_u64_hash_table(&inserter->ssa_definitions_by_ssa_name, get_ssa_name.u16));
    phi_node->local_number = local_number;

    //a1 in the example, will be a define_ssa_node
    struct ssa_control_define_ssa_name_node * extracted_set_local = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node
    );
    int new_version_extracted = allocate_new_version(&inserter->max_version_allocated_per_local, local_number);
    struct ssa_name set_ssa_name = CREATE_SSA_NAME(local_number, new_version_extracted);
    extracted_set_local->ssa_name = CREATE_SSA_NAME(local_number, new_version_extracted);
    extracted_set_local->new_local_value = &phi_node->data;
    put_u64_hash_table(&inserter->ssa_definitions_by_ssa_name, set_ssa_name.u16, extracted_set_local);

    //Insert the new node in the linkedlist of ssa_control_nodes
    extracted_set_local->control.next.next = control_node_to_extract;
    extracted_set_local->control.prev = control_node_to_extract->prev;
    control_node_to_extract->prev->next.next = &extracted_set_local->control;
    control_node_to_extract->prev = &extracted_set_local->control;
    if(to_extract_block->first == control_node_to_extract){
        to_extract_block->first = &extracted_set_local->control;
    }

    add_u64_set(&inserter->extracted_variables_loop_body, (uint64_t) control_node_to_extract);
}