#include "phi_inserter.h"

#define GET_NODES_ALLOCATOR(inserter) (&(inserter)->nodes_lox_allocator->lox_allocator)

struct phi_inserter {
    struct u64_set loops_already_scanned; //Stores lox_ir_block->next_as.loop address
    struct stack_list pending_evaluate;
    bool rescanning_loop_body;
    int rescanning_loop_body_nested_loops;

    struct lox_ir * lox_ir;
};

struct pending_evaluate {
    struct lox_ir_block * pending_block_to_evaluate;
    struct u8_hash_table * parent_versions;
};

static void push_pending_evaluate(struct phi_inserter *, struct lox_ir_block *, struct u8_hash_table*);
static void insert_phis_in_block(struct phi_inserter *, struct lox_ir_block *, struct u8_hash_table*);
static void insert_ssa_versions_in_control_node(struct phi_inserter *, struct lox_ir_block *, struct lox_ir_control_node*,
        struct u8_hash_table*);
static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number);
static void init_phi_inserter(struct phi_inserter*, struct lox_ir*);
static void free_phi_inserter(struct phi_inserter*);
static void extract_get_local(struct phi_inserter*, struct u8_hash_table *parent_versions,
                              struct lox_ir_control_node *control_node_to_extract, struct lox_ir_block *,
                              uint8_t local_number, void ** parent_to_extract_get_local_ptr);
static void put_version(struct u8_hash_table *, uint8_t local_number, uint8_t version);

static bool insert_phis_in_data_node_consumer(
        struct lox_ir_data_node * parent,
        void ** parent_current_ptr,
        struct lox_ir_data_node * current_node,
        void * extra
);

void insert_lox_ir_phis(struct lox_ir * lox_ir) {
    struct phi_inserter inserter;
    init_phi_inserter(&inserter, lox_ir);

    push_pending_evaluate(&inserter, lox_ir->first_block, alloc_u8_hash_table(NATIVE_LOX_ALLOCATOR()));

    while(!is_empty_stack_list(&inserter.pending_evaluate)) {
        struct pending_evaluate * pending_evaluate = pop_stack_list(&inserter.pending_evaluate);
        struct lox_ir_block * block_to_evaluate = pending_evaluate->pending_block_to_evaluate;
        struct u8_hash_table * parent_versions = pending_evaluate->parent_versions;
        NATIVE_LOX_FREE(pending_evaluate);

        insert_phis_in_block(&inserter, block_to_evaluate, parent_versions);

        switch (block_to_evaluate->type_next) {
            case TYPE_NEXT_LOX_IR_BLOCK_SEQ:
                push_pending_evaluate(&inserter, block_to_evaluate->next_as.next, parent_versions);
                break;
            case TYPE_NEXT_LOX_IR_BLOCK_LOOP:
                struct lox_ir_block * to_jump_loop_block = block_to_evaluate->next_as.loop;
                if(contains_u64_set(&inserter.loops_already_scanned, (uint64_t) to_jump_loop_block)){
                    //OP_LOOP always points to the loop jump_to_operand
                    //If an assigment have ocurred inside the loop body, we will need to propagate outside the loop
                    push_pending_evaluate(&inserter, to_jump_loop_block->next_as.branch.false_branch, parent_versions);
                    inserter.rescanning_loop_body = false;
                } else if (!inserter.rescanning_loop_body && !contains_u64_set(&inserter.loops_already_scanned, (uint64_t) to_jump_loop_block)){
                    push_pending_evaluate(&inserter, to_jump_loop_block, parent_versions);
                    add_u64_set(&inserter.loops_already_scanned, (uint64_t) to_jump_loop_block);
                    inserter.rescanning_loop_body_nested_loops = block_to_evaluate->nested_loop_body;
                    inserter.rescanning_loop_body = true;
                }
                break;
            case TYPE_NEXT_LOX_IR_BLOCK_BRANCH:
                if (inserter.rescanning_loop_body &&
                    block_to_evaluate->is_loop_condition &&
                    block_to_evaluate->next_as.branch.false_branch->nested_loop_body < inserter.rescanning_loop_body_nested_loops) {

                    push_pending_evaluate(&inserter, block_to_evaluate->next_as.branch.true_branch, parent_versions);
                } else {
                    push_pending_evaluate(&inserter, block_to_evaluate->next_as.branch.false_branch, clone_u8_hash_table(parent_versions, NATIVE_LOX_ALLOCATOR()));
                    push_pending_evaluate(&inserter, block_to_evaluate->next_as.branch.true_branch, parent_versions);
                }
                break;
            case TYPE_NEXT_LOX_IR_BLOCK_NONE:
                NATIVE_LOX_FREE(parent_versions);
                break;
        }
    }

    free_phi_inserter(&inserter);
}

static void insert_phis_in_block(
        struct phi_inserter * inserter,
        struct lox_ir_block * block,
        struct u8_hash_table * parent_versions
) {
    for(struct lox_ir_control_node * current = block->first;; current = current->next){
        insert_ssa_versions_in_control_node(inserter, block, current, parent_versions);

        if(current == block->last){
            break;
        }
    }
}

struct insert_phis_in_data_node_struct {
    struct lox_ir_control_node * control_node;
    struct u8_hash_table * parent_versions;
    struct phi_inserter * inserter;
    struct lox_ir_block * block;
};

static void insert_ssa_versions_in_control_node(
        struct phi_inserter * inserter,
        struct lox_ir_block * block,
        struct lox_ir_control_node * control_node,
        struct u8_hash_table * parent_versions
) {
    struct insert_phis_in_data_node_struct consumer_struct = (struct insert_phis_in_data_node_struct) {
        .parent_versions = parent_versions,
        .control_node = control_node,
        .inserter = inserter,
        .block = block,
    };

    for_each_data_node_in_lox_ir_control(
            control_node,
            &consumer_struct,
            LOX_IR_DATA_NODE_OPT_POST_ORDER,
            &insert_phis_in_data_node_consumer
    );

    if (control_node->type == LOX_IR_CONTORL_NODE_SET_LOCAL) {
        //set_local control_node_to_lower and define_ssa_name have the same memory outlay, we do this, so we can change the control_node_to_lower easily in the graph
        //without creating any new control_node_to_lower
        struct lox_ir_control_set_local_node * set_local = (struct lox_ir_control_set_local_node *) control_node;
        struct lox_ir_control_define_ssa_name_node * define_ssa_name = (struct lox_ir_control_define_ssa_name_node *) control_node;
        uint8_t local_number = (uint8_t) set_local->local_number;

        define_ssa_name->value = set_local->new_local_value;
        define_ssa_name->control.type = LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME;
        struct ssa_name ssa_name = alloc_ssa_version_lox_ir(inserter->lox_ir, local_number);
        define_ssa_name->ssa_name = ssa_name;

        put_u64_hash_table(&inserter->lox_ir->definitions_by_ssa_name, ssa_name.u16, define_ssa_name);
        put_version(parent_versions, local_number, ssa_name.value.version);
        add_u64_set(&block->defined_ssa_names, ssa_name.u16);
    } else if(control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) control_node;
        put_version(parent_versions, define->ssa_name.value.local_number, define->ssa_name.value.version);
    }
}

static bool insert_phis_in_data_node_consumer(
        struct lox_ir_data_node * parent,
        void ** parent_current_ptr,
        struct lox_ir_data_node * current_node,
        void * extra
) {
    struct insert_phis_in_data_node_struct * consumer_struct = extra;
    struct u8_hash_table * parent_versions = consumer_struct->parent_versions;
    struct lox_ir_control_node * control_node = consumer_struct->control_node;
    struct phi_inserter * inserter = consumer_struct->inserter;
    struct lox_ir_block * block = consumer_struct->block;
    bool inside_expression = parent != NULL;

    //We replace nodes with type LOX_IR_DATA_NODE_GET_LOCAL to LOX_IR_DATA_NODE_PHI
    if (current_node->type == LOX_IR_DATA_NODE_GET_LOCAL) {
        //We don't want to extract a = b; we want to extract only get_locals that are inside an expression
        struct lox_ir_data_get_local_node * get_local = (struct lox_ir_data_get_local_node *) current_node;
        uint8_t local_number = get_local->local_number;

        if(BELONGS_TO_LOOP_BODY_BLOCK(block) &&
           !block->is_loop_condition &&
           inside_expression &&
           contains_u8_set(&block->use_before_assigment, local_number) //TODO Check if the local has not been already extrated
        ){
            extract_get_local(inserter, parent_versions, control_node, block, local_number, parent_current_ptr);
        } else {
            //We are going to replace the OP_GET_LOCAL control_node_to_lower with a phi control_node_to_lower
            struct lox_ir_data_phi_node * phi_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_PHI, struct lox_ir_data_phi_node,
                    current_node->original_bytecode, LOX_IR_ALLOCATOR(consumer_struct->inserter->lox_ir));
            phi_node->local_number = local_number;
            init_u64_set(&phi_node->ssa_versions, NATIVE_LOX_ALLOCATOR());
            uint8_t version = get_version(parent_versions, get_local->local_number);
            add_u64_set(&phi_node->ssa_versions, version);

            //Replace parent pointer to child control_node_to_lower
            //OP_GET_LOCAL will always be a child of another data control_node_to_lower.
            *parent_current_ptr = &phi_node->data;
        }
    } else if (current_node->type == LOX_IR_DATA_NODE_PHI) {
        struct lox_ir_data_phi_node * phi_node = (struct lox_ir_data_phi_node *) current_node;
        uint8_t last_version = get_version(parent_versions, phi_node->local_number);

        //Prevents this issue: i2 = phi(i0, i2) + 1 when inserting phi function in loop bodies
        if(control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME &&
           ((struct lox_ir_control_define_ssa_name_node *) control_node)->ssa_name.value.version == last_version){
            return true;
        }


        add_u64_set(&phi_node->ssa_versions, last_version);
    }

    return true;
}

static void put_version(struct u8_hash_table * parent_versions, uint8_t local_number, uint8_t version) {
    //Inserting a NULL value in the u8_hash_table, it means that the slot is emtpy. That is why we add 1 to the version
    put_u8_hash_table(parent_versions, local_number, (void *) version + 1);
}

static int get_version(struct u8_hash_table * parent_versions, uint8_t local_number) {
    // -1 to get the propper version. Inserting a NULL value in the u8_hash_table, it means that the slot is emtpy
    if(contains_u8_hash_table(parent_versions, local_number)){
        return ((int) get_u8_hash_table(parent_versions, local_number)) - 1;
    } else {
        put_u8_hash_table(parent_versions, local_number, (void *) 0x01);
        return 0;
    }
}

static void push_pending_evaluate(
        struct phi_inserter * inserter,
        struct lox_ir_block * parent,
        struct u8_hash_table * parent_versions
) {
    struct pending_evaluate * pending_evaluate = NATIVE_LOX_MALLOC(sizeof(struct pending_evaluate));
    pending_evaluate->parent_versions = parent_versions;
    pending_evaluate->pending_block_to_evaluate = parent;
    push_stack_list(&inserter->pending_evaluate, pending_evaluate);
}

//TODO Add local lox arena allocator
static void init_phi_inserter(struct phi_inserter * phi_inserter, struct lox_ir * lox_ir) {
    init_u64_set(&phi_inserter->loops_already_scanned, NATIVE_LOX_ALLOCATOR());
    init_stack_list(&phi_inserter->pending_evaluate, NATIVE_LOX_ALLOCATOR());
    phi_inserter->rescanning_loop_body = false;
    phi_inserter->lox_ir = lox_ir;
}

static void free_phi_inserter(struct phi_inserter * inserter) {
    free_stack_list(&inserter->pending_evaluate);
    free_u64_set(&inserter->loops_already_scanned);
}

//a = a + 1; will be replaced: a1 = phi(a0); a2 = phi(a1) + 1;
static void extract_get_local(
        struct phi_inserter * inserter,
        struct u8_hash_table * parent_versions,
        struct lox_ir_control_node * control_node_to_extract,
        struct lox_ir_block * to_extract_block,
        uint8_t local_number,
        void ** parent_to_extract_get_local_ptr
) {
    //a0 in the example, will be a phi control_node_to_lower
    struct lox_ir_data_phi_node * extracted_phi_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_PHI, struct lox_ir_data_phi_node,
            NULL, LOX_IR_ALLOCATOR(inserter->lox_ir));

    init_u64_set(&extracted_phi_node->ssa_versions, NATIVE_LOX_ALLOCATOR());
    add_u64_set(&extracted_phi_node->ssa_versions, get_version(parent_versions, local_number));
    extracted_phi_node->local_number = local_number;

    //a1 in the example, will be a define_ssa_node
    struct lox_ir_control_define_ssa_name_node * extracted_set_local = ALLOC_LOX_IR_CONTROL(LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME,
            struct lox_ir_control_define_ssa_name_node, to_extract_block, LOX_IR_ALLOCATOR(inserter->lox_ir));

    struct ssa_name new_ssa_name_extracted = alloc_ssa_version_lox_ir(inserter->lox_ir, local_number);
    extracted_set_local->ssa_name = CREATE_SSA_NAME(local_number, new_ssa_name_extracted.value.version);
    extracted_set_local->value = &extracted_phi_node->data;
    put_u64_hash_table(&inserter->lox_ir->definitions_by_ssa_name, new_ssa_name_extracted.u16, extracted_set_local);
    add_u64_set(&to_extract_block->defined_ssa_names, new_ssa_name_extracted.u16);

    //phi(a1) in the example, will be a define_ssa_node. Replace get_local control_node_to_lower with phi control_node_to_lower
    struct lox_ir_data_phi_node * new_get_local = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_PHI, struct lox_ir_data_phi_node, NULL,
            LOX_IR_ALLOCATOR(inserter->lox_ir));
    init_u64_set(&new_get_local->ssa_versions, NATIVE_LOX_ALLOCATOR());
    new_get_local->local_number = local_number;
    add_u64_set(&new_get_local->ssa_versions, new_ssa_name_extracted.value.version);
    *parent_to_extract_get_local_ptr = &new_get_local->data;

    //Insert the new control_node_to_lower in the linkedlist of control_nodes
    extracted_set_local->control.next = control_node_to_extract;
    extracted_set_local->control.prev = control_node_to_extract->prev;
    if(control_node_to_extract->prev != NULL){
        control_node_to_extract->prev->next = &extracted_set_local->control;
    }
    control_node_to_extract->prev = &extracted_set_local->control;
    if(to_extract_block->first == control_node_to_extract){
        to_extract_block->first = &extracted_set_local->control;
    }
}