#include "unboxing_insertion.h"

struct ui {
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator ui_allocator;
};

static struct ui * alloc_unbox_insertion(struct ssa_ir *);
static void free_unbox_insertion(struct ui*);
static bool control_requires_boxed_input(struct ui*, struct ssa_control_node *);
static bool data_requires_boxed_input(struct ssa_data_node *);
static bool ssa_names_requires_to_be_boxed(struct ui*, struct ssa_name);

static bool perform_unboxing_insertion_block(struct ssa_block *, void *);
static void perform_unboxing_insertion_control(struct ui *, struct ssa_block *, struct ssa_control_node *);
static bool perform_unboxing_insertion_data_consumer(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static void perform_unboxing_insertion_data(struct ui*, struct ssa_block*, struct ssa_control_node*, struct ssa_data_node*, struct ssa_data_node*, void**, bool);
static void insert_box_node(struct ui*, struct ssa_data_node *, void **);
static void insert_unbox_node(struct ui*, struct ssa_data_node *, void **);
static void unbox_terminator_data_node(struct ui*, struct ssa_block*, struct ssa_control_node*, struct ssa_data_node*, void**);
static void unbox_non_terminator_data_node(struct ui *, struct ssa_data_node*, void**);
static bool is_ssa_name_unboxed(struct ui*, struct ssa_block*, struct ssa_name);
static bool is_ssa_name_boxed(struct ui*, struct ssa_block*, struct ssa_name);
static void extract_define_unboxed_from_phi(struct ui*, struct ssa_block*, struct ssa_control_node*, struct ssa_data_phi_node*, struct ssa_name);
static void extract_define_boxed_from_phi(struct ui*, struct ssa_block*, struct ssa_control_node*, struct ssa_data_phi_node*, struct ssa_name);

void perform_unboxing_insertion(struct ssa_ir * ssa_ir) {
    struct ui * ui = alloc_unbox_insertion(ssa_ir);

    for_each_ssa_block(
            ssa_ir->first_block,
            &ui->ui_allocator.lox_allocator,
            ui,
            SSA_BLOCK_OPT_NOT_REPEATED,
            &perform_unboxing_insertion_block
    );

    free_unbox_insertion(ui);
}

static bool perform_unboxing_insertion_block(struct ssa_block * current_block, void * extra) {
    struct ui * ui = extra;

    for(struct ssa_control_node * current_control = current_block->first;; current_control = current_control->next){
        perform_unboxing_insertion_control(ui, current_block, current_control);

        if (current_control == current_block->last) {
            break;
        }
    }

    return true;
}

struct perform_unboxing_insertion_data_struct {
    bool should_produced_boxed_result;
    struct ssa_control_node * control_node;
    struct ssa_block * block;
    struct ui * ui;
};

static void perform_unboxing_insertion_control(
        struct ui * ui,
        struct ssa_block * current_block,
        struct ssa_control_node * current_control
) {
    struct perform_unboxing_insertion_data_struct consumer_struct = (struct perform_unboxing_insertion_data_struct) {
        .should_produced_boxed_result = control_requires_boxed_input(ui, current_control),
        .control_node = current_control,
        .block = current_block,
        .ui = ui,
    };

    for_each_data_node_in_control_node(
            current_control,
            &consumer_struct,
            SSA_DATA_NODE_OPT_PRE_ORDER,
            &perform_unboxing_insertion_data_consumer
    );

    if (current_control->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME) {
        struct ssa_control_define_ssa_name_node * define = (struct ssa_control_define_ssa_name_node *) current_control;
        put_type_by_ssa_name_ssa_ir(ui->ssa_ir, current_block, define->ssa_name, define->value->produced_type);
    }
}

static bool perform_unboxing_insertion_data_consumer(
        struct ssa_data_node * parent,
        void ** parent_child_ptr,
        struct ssa_data_node * child,
        void * extra
) {
    struct perform_unboxing_insertion_data_struct * consumer_struct = extra;
    bool should_produed_boxed_result = consumer_struct->should_produced_boxed_result;
    perform_unboxing_insertion_data(consumer_struct->ui, consumer_struct->block, consumer_struct->control_node,
        child, parent, parent_child_ptr, should_produed_boxed_result);

    //Only iterate parent
    return false;
}

static void perform_unboxing_insertion_data(
        struct ui * ui,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node,
        struct ssa_data_node * parent,
        void ** parent_data_node_ptr,
        bool data_node_should_produce_boxed_output
) {
    bool data_node_requires_boxed_inputs = data_requires_boxed_input(data_node);

    FOR_EACH_U64_SET_VALUE(get_children_ssa_data_node(data_node, &ui->ui_allocator.lox_allocator), children_field_ptr_u64) {
        void ** children_field_ptr = (void **) children_field_ptr_u64;
        struct ssa_data_node * child = *((struct ssa_data_node **) children_field_ptr);

        perform_unboxing_insertion_data(ui, block, control_node, child, data_node,
            (void**) children_field_ptr, data_node_requires_boxed_inputs);
    }

    //Innecesary to add unbox/box node in conditional jump
    if(parent == NULL && control_node->type == SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP){
        return;
    }

    if (!data_node_should_produce_boxed_output && is_terminator_ssa_data_node(data_node)) {
        unbox_terminator_data_node(ui, block, control_node, data_node, parent_data_node_ptr);
    } else if (!data_node_should_produce_boxed_output && !is_terminator_ssa_data_node(data_node)) {
        unbox_non_terminator_data_node(ui, data_node, parent_data_node_ptr);
    } else if (data_node_should_produce_boxed_output){
        insert_box_node(ui, data_node, parent_data_node_ptr);
    }
}

static void unbox_non_terminator_data_node(struct ui * ui, struct ssa_data_node * data_node, void ** data_node_field_ptr) {
    if(is_native_ssa_type(data_node->produced_type->type) || data_node->produced_type->type == SSA_TYPE_F64){
        return;
    }

    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) data_node;
            //Always produced native type
            binary->data.produced_type = CREATE_SSA_TYPE(binary_to_ssa_type(binary->operator, binary->left->produced_type->type,
                binary->right->produced_type->type, RETURN_NATIVE_TYPE_AS_DEFAULT), SSA_IR_ALLOCATOR(ui->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_CALL: {
            insert_unbox_node(ui, data_node, data_node_field_ptr);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            data_node->produced_type->type = SSA_TYPE_NATIVE_I64;
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            data_node->produced_type->type = SSA_TYPE_NATIVE_STRUCT_INSTANCE;
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            data_node->produced_type->type = SSA_TYPE_NATIVE_ARRAY;
            break;
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) data_node;
            unary->data.produced_type = unary->data.produced_type;
            insert_unbox_node(ui, data_node, data_node_field_ptr);
            break;
        }
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_guard * guard = (struct ssa_guard *) data_node;
            if (guard->type == SSA_GUARD_TYPE_CHECK) {
                insert_unbox_node(ui, data_node, data_node_field_ptr);
            }
            break;
        }
    }
}

static void unbox_terminator_data_node(
        struct ui * ui,
        struct ssa_block * block,
        struct ssa_control_node * control_node,
        struct ssa_data_node * data_node,
        void ** data_node_field_ptr
) {
    if (is_native_ssa_type(data_node->produced_type->type) || data_node->produced_type->type == SSA_TYPE_F64) {
        return;
    }

    switch (data_node->type) {
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) data_node;
            bool produced_any = phi->data.produced_type->type == SSA_TYPE_LOX_ANY;
            bool phi_node_should_produce_boxed = produced_any;
            bool phi_node_should_produce_unboxed = !produced_any;

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, phi_ssa_name) {
                if (is_ssa_name_boxed(ui, block, phi_ssa_name) && phi_node_should_produce_unboxed) {
                    extract_define_unboxed_from_phi(ui, block, control_node, phi, phi_ssa_name);
                } else if(is_ssa_name_unboxed(ui, block, phi_ssa_name) && phi_node_should_produce_boxed) {
                    extract_define_boxed_from_phi(ui, block, control_node, phi, phi_ssa_name);
                }
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) data_node;
            get_ssa_name->data.produced_type = get_type_by_ssa_name_ssa_ir(ui->ssa_ir, block, get_ssa_name->ssa_name);

            if (!is_ssa_name_unboxed(ui, block, get_ssa_name->ssa_name)) {
                insert_unbox_node(ui, data_node, data_node_field_ptr);
            }
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            unbox_const_ssa_data_node((struct ssa_data_constant_node *) data_node);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            insert_unbox_node(ui, data_node, data_node_field_ptr);
            break;
        }
        default: {
            //TODO Runtime panic
        }
    }
}

//Given a2 = phi(a0, a1) Extract: a1, it will produce: a3 = unbox(a1); a2 = phi(a0, a3)
static void extract_define_boxed_from_phi(
        struct ui * ui,
        struct ssa_block * block,
        struct ssa_control_node * control_uses_phi,
        struct ssa_data_phi_node * phi_node,
        struct ssa_name ssa_name_to_extract
) {
    struct ssa_name unboxed_ssa_name = alloc_ssa_version_ssa_ir(ui->ssa_ir, ssa_name_to_extract.value.local_number);

    struct ssa_control_define_ssa_name_node * define_boxed = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node, block, SSA_IR_ALLOCATOR(ui->ssa_ir)
    );
    struct ssa_data_node_box * box_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_BOX, struct ssa_data_node_box, NULL, SSA_IR_ALLOCATOR(ui->ssa_ir)
    );
    struct ssa_data_get_ssa_name_node * get_boxed_ssa_name_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, SSA_IR_ALLOCATOR(ui->ssa_ir)
    );

    //define_boxed
    define_boxed->ssa_name = unboxed_ssa_name;
    define_boxed->value = &box_node->data;
    add_u64_set(&block->defined_ssa_names, unboxed_ssa_name.u16);
    put_u64_hash_table(&ui->ssa_ir->ssa_definitions_by_ssa_name, unboxed_ssa_name.u16, define_boxed);
    add_before_control_node_ssa_block(block, control_uses_phi, &define_boxed->control);

    //get_boxed_ssa_name_node
    get_boxed_ssa_name_node->data.produced_type = get_type_by_ssa_name_ssa_ir(ui->ssa_ir, block, ssa_name_to_extract);
    get_boxed_ssa_name_node->ssa_name = ssa_name_to_extract;
    add_ssa_name_use_ssa_ir(ui->ssa_ir, ssa_name_to_extract, &define_boxed->control);

    //box_node
    box_node->to_box = &get_boxed_ssa_name_node->data;
    box_node->data.produced_type = native_to_lox_ssa_type(get_boxed_ssa_name_node->data.produced_type, SSA_IR_ALLOCATOR(ui->ssa_ir));

    //control_uses_phi
    remove_u64_set(&phi_node->ssa_versions, ssa_name_to_extract.value.version);
    add_u64_set(&phi_node->ssa_versions, unboxed_ssa_name.value.version);
    add_ssa_name_use_ssa_ir(ui->ssa_ir, unboxed_ssa_name, &define_boxed->control);
    remove_ssa_name_use_ssa_ir(ui->ssa_ir, ssa_name_to_extract, control_uses_phi);
}

//Given a2 = phi(a0, a1) Extract: a1, it will produce: a3 = unbox(a1); a2 = phi(a0, a3)
static void extract_define_unboxed_from_phi(
        struct ui * ui,
        struct ssa_block * block,
        struct ssa_control_node * control_uses_phi,
        struct ssa_data_phi_node * phi_node,
        struct ssa_name ssa_name_to_extract
) {
    struct ssa_name unboxed_ssa_name = alloc_ssa_version_ssa_ir(ui->ssa_ir, ssa_name_to_extract.value.local_number);

    struct ssa_control_define_ssa_name_node * define_unboxed = ALLOC_SSA_CONTROL_NODE(
            SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME, struct ssa_control_define_ssa_name_node, block, SSA_IR_ALLOCATOR(ui->ssa_ir)
    );
    struct ssa_data_node_unbox * unbox_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_UNBOX, struct ssa_data_node_unbox, NULL, SSA_IR_ALLOCATOR(ui->ssa_ir)
    );
    struct ssa_data_get_ssa_name_node * get_boxed_ssa_name_node = ALLOC_SSA_DATA_NODE(
            SSA_DATA_NODE_TYPE_GET_SSA_NAME, struct ssa_data_get_ssa_name_node, NULL, SSA_IR_ALLOCATOR(ui->ssa_ir)
    );

    //define_unboxed
    define_unboxed->ssa_name = unboxed_ssa_name;
    define_unboxed->value = &unbox_node->data;
    add_u64_set(&block->defined_ssa_names, unboxed_ssa_name.u16);
    put_u64_hash_table(&ui->ssa_ir->ssa_definitions_by_ssa_name, unboxed_ssa_name.u16, define_unboxed);
    add_before_control_node_ssa_block(block, control_uses_phi, &define_unboxed->control);

    //get_boxed_ssa_name_node
    get_boxed_ssa_name_node->data.produced_type = get_type_by_ssa_name_ssa_ir(ui->ssa_ir, block, ssa_name_to_extract);
    get_boxed_ssa_name_node->ssa_name = ssa_name_to_extract;
    add_ssa_name_use_ssa_ir(ui->ssa_ir, ssa_name_to_extract, &define_unboxed->control);

    //unbox_node
    unbox_node->to_unbox = &get_boxed_ssa_name_node->data;
    unbox_node->data.produced_type = lox_to_native_ssa_type(get_boxed_ssa_name_node->data.produced_type, SSA_IR_ALLOCATOR(ui->ssa_ir));

    //control_uses_phi
    remove_u64_set(&phi_node->ssa_versions, ssa_name_to_extract.value.version);
    add_u64_set(&phi_node->ssa_versions, unboxed_ssa_name.value.version);
    add_ssa_name_use_ssa_ir(ui->ssa_ir, unboxed_ssa_name, &define_unboxed->control);
    remove_ssa_name_use_ssa_ir(ui->ssa_ir, ssa_name_to_extract, control_uses_phi);
}

static void insert_box_node(struct ui * ui, struct ssa_data_node * data_node, void ** data_node_field_ptr) {
    if(is_native_ssa_type(data_node->produced_type->type)){
        struct ssa_data_node_box * box = ALLOC_SSA_DATA_NODE(
                SSA_DATA_NODE_TYPE_BOX, struct ssa_data_node_box, NULL, SSA_IR_ALLOCATOR(ui->ssa_ir)
        );
        box->data.produced_type = native_to_lox_ssa_type(data_node->produced_type, SSA_IR_ALLOCATOR(ui->ssa_ir));
        box->to_box = data_node;

        *data_node_field_ptr = box;
    }
}

static void insert_unbox_node(struct ui * ui, struct ssa_data_node * data_node, void ** data_node_field_ptr) {
    if (is_lox_ssa_type(data_node->produced_type->type) && data_node->produced_type->type != SSA_TYPE_LOX_ANY) {
        struct ssa_data_node_unbox * unbox = ALLOC_SSA_DATA_NODE(
                SSA_DATA_NODE_TYPE_UNBOX, struct ssa_data_node_unbox, NULL, SSA_IR_ALLOCATOR(ui->ssa_ir)
        );
        unbox->data.produced_type = lox_to_native_ssa_type(data_node->produced_type, SSA_IR_ALLOCATOR(ui->ssa_ir));
        unbox->to_unbox = data_node;

        *data_node_field_ptr = unbox;
    }
}

static bool is_ssa_name_boxed(struct ui * ui, struct ssa_block * block, struct ssa_name ssa_name) {
    return !is_ssa_name_unboxed(ui, block, ssa_name);
}

static bool is_ssa_name_unboxed(struct ui * ui, struct ssa_block * block, struct ssa_name ssa_name) {
    struct ssa_type * type = get_type_by_ssa_name_ssa_ir(ui->ssa_ir, block, ssa_name);
    return type->type == SSA_TYPE_F64 || is_native_ssa_type(type->type);
}

static bool control_requires_boxed_input(struct ui * ui, struct ssa_control_node * control) {
    switch (control->type) {
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
        case SSA_CONTROL_NODE_TYPE_RETURN:
            return true;
        case SSA_CONTROL_NODE_TYPE_PRINT:
        case SSA_CONTROL_NODE_TYPE_DATA:
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL:
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
        case SSA_CONTROL_NODE_GUARD:
            return false;
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME: {
            struct ssa_control_define_ssa_name_node * define = (struct ssa_control_define_ssa_name_node *) control;
            struct u64_set * uses_ssa_name = get_u64_hash_table(&ui->ssa_ir->node_uses_by_ssa_name, define->ssa_name.u16);
            if(uses_ssa_name == NULL){
                return false;
            }

            bool some_use_required_unboxed = false;

            FOR_EACH_U64_SET_VALUE(*uses_ssa_name, node_uses_ssa_name_ptr_u64) {
                struct ssa_control_node * node_uses_ssa_name = (struct ssa_control_node *) node_uses_ssa_name_ptr_u64;

                if(node_uses_ssa_name->type != SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT &&
                        node_uses_ssa_name->type != SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD &&
                        node_uses_ssa_name->type != SSA_CONTORL_NODE_TYPE_SET_GLOBAL &&
                        node_uses_ssa_name->type != SSA_CONTROL_NODE_TYPE_RETURN){
                    some_use_required_unboxed = true;
                    break;
                }
            }

            return !some_use_required_unboxed;
        }
    }
}

static bool data_requires_boxed_input(struct ssa_data_node * data) {
    switch (data->type) {
        case SSA_DATA_NODE_TYPE_CALL:
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
        case SSA_DATA_NODE_TYPE_UNBOX:
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
            return true;
        case SSA_DATA_NODE_TYPE_GET_LOCAL:
        case SSA_DATA_NODE_TYPE_GET_GLOBAL:
        case SSA_DATA_NODE_TYPE_CONSTANT:
        case SSA_DATA_NODE_TYPE_UNARY:
        case SSA_DATA_NODE_TYPE_BOX:
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH:
        case SSA_DATA_NODE_TYPE_GUARD:
        case SSA_DATA_NODE_TYPE_PHI:
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
            return false;
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) data;
            return binary->left->produced_type->type == SSA_TYPE_LOX_ANY || binary->right->produced_type->type == SSA_TYPE_LOX_ANY;
        }
    }
}

static bool ssa_names_requires_to_be_boxed(struct ui * ui, struct ssa_name ssa_name) {
    struct u64_set * ssa_name_uses_set = get_u64_hash_table(&ui->ssa_ir->node_uses_by_ssa_name, ssa_name.u16);
    bool input_requires_to_be_boxed = false;

    FOR_EACH_U64_SET_VALUE(*ssa_name_uses_set, control_nodes_uses_ssa_name_u64_ptr) {
        struct ssa_control_node * control_nodes_uses_ssa_name = (struct ssa_control_node *) control_nodes_uses_ssa_name_u64_ptr;

        if (control_requires_boxed_input(ui, control_nodes_uses_ssa_name)) {
            input_requires_to_be_boxed = true;
            break;
        }
    }

    return input_requires_to_be_boxed;
}

static struct ui * alloc_unbox_insertion(struct ssa_ir * ssa_ir) {
    struct ui * ui =  NATIVE_LOX_MALLOC(sizeof(struct ui));
    ui->ssa_ir = ssa_ir;
    struct arena arena;
    init_arena(&arena);
    ui->ui_allocator = to_lox_allocator_arena(arena);

    return ui;
}

static void free_unbox_insertion(struct ui * ui) {
    free_arena(&ui->ui_allocator.arena);
    NATIVE_LOX_FREE(ui);
}