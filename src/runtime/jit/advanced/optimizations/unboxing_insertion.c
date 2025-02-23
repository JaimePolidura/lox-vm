    #include "unboxing_insertion.h"

struct ui {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator ui_allocator;
};

static struct ui * alloc_unbox_insertion(struct lox_ir *);
static void free_unbox_insertion(struct ui*);
static bool control_requires_lox_input(struct ui *ui, struct lox_ir_control_node *control);

static bool perform_unboxing_insertion_block(struct lox_ir_block *, void *);
static void perform_unboxing_insertion_control(struct ui *, struct lox_ir_block *, struct lox_ir_control_node *);
static bool perform_unboxing_insertion_data_consumer(struct lox_ir_data_node*, void**, struct lox_ir_data_node*, void*);
static void perform_unboxing_insertion_data(struct ui*, struct lox_ir_block*, struct lox_ir_control_node*, struct lox_ir_data_node*,
        struct lox_ir_data_node*, void**, lox_ir_type_t type_parent_should_produce);
static bool is_ssa_name_unboxed(struct ui*, struct lox_ir_block*, struct ssa_name);
static void extract_define_cast_from_phi(struct ui*,struct lox_ir_block*,struct lox_ir_control_node*,struct lox_ir_data_phi_node *,
        struct ssa_name,lox_ir_type_t);
static lox_ir_type_t calculate_type_should_produce_data(struct ui*,struct lox_ir_block*,struct lox_ir_data_node*,bool);
static lox_ir_type_t calculate_data_input_type_should_produce(struct ui*,struct lox_ir_block*,struct lox_ir_data_node*,struct lox_ir_data_node*,bool);
static struct lox_ir_data_node * insert_lox_type_check_guard(struct ui*,struct lox_ir_data_node*,void**,lox_ir_type_t);
static void insert_cast_node(struct ui*,struct lox_ir_data_node*,void**,lox_ir_type_t);
static void cast_node(struct ui*,struct lox_ir_data_node*,struct lox_ir_control_node*,void**,lox_ir_type_t);

void perform_unboxing_insertion(struct lox_ir * lox_ir) {
    struct ui * ui = alloc_unbox_insertion(lox_ir);

    for_each_lox_ir_block(
            lox_ir->first_block,
            &ui->ui_allocator.lox_allocator,
            ui,
            LOX_IR_BLOCK_OPT_NOT_REPEATED,
            &perform_unboxing_insertion_block
    );

    free_unbox_insertion(ui);
}

static bool perform_unboxing_insertion_block(struct lox_ir_block * current_block, void * extra) {
    struct ui * ui = extra;

    for(struct lox_ir_control_node * current_control = current_block->first;; current_control = current_control->next){
        perform_unboxing_insertion_control(ui, current_block, current_control);

        if (current_control == current_block->last) {
            break;
        }
    }

    return true;
}

struct perform_unboxing_insertion_data_struct {
    bool should_produced_boxed_result;
    struct lox_ir_control_node * control_node;
    struct lox_ir_block * block;
    struct ui * ui;
};

static void perform_unboxing_insertion_control(
        struct ui * ui,
        struct lox_ir_block * current_block,
        struct lox_ir_control_node * current_control
) {
    struct perform_unboxing_insertion_data_struct consumer_struct = (struct perform_unboxing_insertion_data_struct) {
        .should_produced_boxed_result = control_requires_lox_input(ui, current_control),
        .control_node = current_control,
        .block = current_block,
        .ui = ui,
    };

    for_each_data_node_in_lox_ir_control(
            current_control,
            &consumer_struct,
            LOX_IR_DATA_NODE_OPT_PRE_ORDER,
            &perform_unboxing_insertion_data_consumer
    );

    if (current_control->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) current_control;
        put_type_by_ssa_name_lox_ir(ui->lox_ir, current_block, define->ssa_name, define->value->produced_type);
    }
}

static bool perform_unboxing_insertion_data_consumer(
        struct lox_ir_data_node * parent,
        void ** parent_child_ptr,
        struct lox_ir_data_node * child,
        void * extra
) {
    struct perform_unboxing_insertion_data_struct * consumer_struct = extra;
    lox_ir_type_t type_should_produce = control_requires_lox_input(consumer_struct->ui, consumer_struct->control_node) ?
        LOX_IR_TYPE_LOX_ANY : LOX_IR_TYPE_UNKNOWN;

    perform_unboxing_insertion_data(consumer_struct->ui, consumer_struct->block, consumer_struct->control_node,
        child, parent, parent_child_ptr, type_should_produce);

    //Only iterate parent
    return false;
}

static void perform_unboxing_insertion_data(
        struct ui * ui,
        struct lox_ir_block * block,
        struct lox_ir_control_node * control_node,
        struct lox_ir_data_node * data_node,
        struct lox_ir_data_node * parent_node,
        void ** parent_data_node_ptr,
        lox_ir_type_t type_should_produce
) {
    bool first_iteration = parent_node == NULL;
    bool should_produce_lox = is_lox_lox_ir_type(type_should_produce);

    FOR_EACH_U64_SET_VALUE(get_children_lox_ir_data_node(data_node, &ui->ui_allocator.lox_allocator), children_field_ptr_u64) {
        void ** children_field_ptr = (void **) children_field_ptr_u64;
        struct lox_ir_data_node * child = *((struct lox_ir_data_node **) children_field_ptr);

        lox_ir_type_t type_child_should_produce = calculate_data_input_type_should_produce(ui, block, data_node, child, should_produce_lox);

        perform_unboxing_insertion_data(ui, block, control_node, child, data_node, (void**) children_field_ptr,
                                        type_child_should_produce);
    }

    type_should_produce = parent_node == NULL ?
            calculate_type_should_produce_data(ui, block, data_node, should_produce_lox) : type_should_produce;

    //Special case: We deal with string concatenation by emitting at compiletime a functino call to generic function to
    //concatenate strings, so we won't cast its operands
    bool is_string_concatenation_case = parent_node != NULL
            && parent_node->type == LOX_IR_DATA_NODE_BINARY
            && ((struct lox_ir_data_binary_node *) parent_node)->operator == OP_ADD
            && IS_STRING_LOX_IR_TYPE(parent_node->produced_type->type);

    if (type_should_produce != data_node->produced_type->type && !is_string_concatenation_case) {
        cast_node(ui, data_node, control_node, parent_data_node_ptr, type_should_produce);
    }
}

static void cast_node(
        struct ui * ui,
        struct lox_ir_data_node * data_node,
        struct lox_ir_control_node * control_node,
        void ** parent_field_ptr,
        lox_ir_type_t type_should_produce
) {
    bool native_to_lox_cast = is_lox_lox_ir_type(type_should_produce) || type_should_produce == LOX_IR_TYPE_F64;
    bool lox_to_native_cast = !native_to_lox_cast;
    struct lox_ir_block * block = control_node->block;

    switch (data_node->type) {
        case LOX_IR_DATA_NODE_CONSTANT: {
            if (lox_to_native_cast) {
                unbox_const_lox_ir_data_node((struct lox_ir_data_constant_node *) data_node);
            }
            break;
        }
        case LOX_IR_DATA_NODE_CALL:
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            insert_cast_node(ui, data_node, parent_field_ptr, type_should_produce);
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT:
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD:
        case LOX_IR_DATA_NODE_GUARD: {
            insert_cast_node(ui, data_node, parent_field_ptr, type_should_produce);
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            data_node->produced_type->type = native_to_lox_cast ? LOX_IR_TYPE_LOX_STRUCT_INSTANCE : LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            data_node->produced_type->type = LOX_IR_TYPE_NATIVE_I64;
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            data_node->produced_type->type = native_to_lox_cast ? LOX_IR_TYPE_LOX_ARRAY : LOX_IR_TYPE_NATIVE_ARRAY;
            break;
        }
        case LOX_IR_DATA_NODE_BINARY:
        case LOX_IR_DATA_NODE_UNARY: {
            data_node->produced_type->type = type_should_produce;
            break;
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            data_node->produced_type = get_type_by_ssa_name_lox_ir(ui->lox_ir, block, GET_USED_SSA_NAME(data_node));
            if (data_node->produced_type->type != type_should_produce) {
                insert_cast_node(ui, data_node, parent_field_ptr, type_should_produce);
            }
            break;
        }
        case LOX_IR_DATA_NODE_PHI: {
            struct lox_ir_data_phi_node * phi = (struct lox_ir_data_phi_node *) data_node;
            bool produced_any = phi->data.produced_type->type == LOX_IR_TYPE_LOX_ANY;
            bool phi_node_should_produce_native = !produced_any;
            bool phi_node_should_produce_lox = produced_any;

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, phi_ssa_name) {
                bool is_ssa_name_native = is_ssa_name_unboxed(ui, block, phi_ssa_name);
                bool needs_to_be_extracted = (is_ssa_name_native && phi_node_should_produce_lox)
                        || (!is_ssa_name_native && phi_node_should_produce_native);

                if (needs_to_be_extracted) {
                    extract_define_cast_from_phi(ui, block, control_node, phi, phi_ssa_name, type_should_produce);
                }
            }
            break;
        }
    }
}

static void insert_cast_node(
        struct ui * ui,
        struct lox_ir_data_node * to_cast,
        void ** parent,
        lox_ir_type_t type_should_produce
) {
    if(to_cast->produced_type->type == LOX_IR_TYPE_LOX_ANY){
        struct lox_ir_data_node * guard_node = insert_lox_type_check_guard(ui, to_cast, parent, type_should_produce);
        *parent = (void *) guard_node;
        return;
    }

    struct lox_ir_data_cast_node * cast_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_CAST, struct lox_ir_data_cast_node,
            NULL, LOX_IR_ALLOCATOR(ui->lox_ir));
    cast_node->to_cast = to_cast;
    cast_node->data.produced_type = create_lox_ir_type(type_should_produce, LOX_IR_ALLOCATOR(ui->lox_ir));

    *parent = (void *) cast_node;
}

static struct lox_ir_data_node * insert_lox_type_check_guard(
        struct ui * ui,
        struct lox_ir_data_node * node,
        void ** parnet_field_ptr,
        lox_ir_type_t type_to_be_checked
) {
    struct lox_ir_data_guard_node * guard_node = ALLOC_LOX_IR_DATA(LOX_IR_DATA_NODE_GUARD, struct lox_ir_data_guard_node, NULL,
        LOX_IR_ALLOCATOR(ui->lox_ir));

    guard_node->guard.value = node;
    guard_node->guard.type = LOX_IR_GUARD_TYPE_CHECK;
    guard_node->guard.value_to_compare.type = type_to_be_checked;
    guard_node->guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER;
    guard_node->data.produced_type = create_lox_ir_type(type_to_be_checked, LOX_IR_ALLOCATOR(ui->lox_ir));

    if(parnet_field_ptr != NULL){
        *parnet_field_ptr = (void *) guard_node;
    }

    return &guard_node->data;
}

//Given a2 = phi(a0, a1) Extract: a1, it will produce: a3 = cast(a1); a2 = phi(a0, a3)
static void extract_define_cast_from_phi(
        struct ui * ui,
        struct lox_ir_block * block,
        struct lox_ir_control_node * control_uses_phi,
        struct lox_ir_data_phi_node * phi_node,
        struct ssa_name ssa_name_to_extract,
        lox_ir_type_t type_to_be_casted
) {
    struct ssa_name casted_ssa_name = alloc_ssa_version_lox_ir(ui->lox_ir, ssa_name_to_extract.value.local_number);

    struct lox_ir_control_define_ssa_name_node * define_casted = ALLOC_LOX_IR_CONTROL(
            LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME, struct lox_ir_control_define_ssa_name_node, block, LOX_IR_ALLOCATOR(ui->lox_ir)
    );
    struct lox_ir_data_get_ssa_name_node * get_uncasted_ssa_name_node = ALLOC_LOX_IR_DATA(
            LOX_IR_DATA_NODE_GET_SSA_NAME, struct lox_ir_data_get_ssa_name_node, NULL, LOX_IR_ALLOCATOR(ui->lox_ir)
    );

    //get_uncasted_ssa_name_node
    get_uncasted_ssa_name_node->data.produced_type = get_type_by_ssa_name_lox_ir(ui->lox_ir, block, ssa_name_to_extract);
    get_uncasted_ssa_name_node->ssa_name = ssa_name_to_extract;
    add_ssa_name_use_lox_ir(ui->lox_ir, ssa_name_to_extract, &define_casted->control);

    //define casted node
    define_casted->ssa_name = casted_ssa_name;
    define_casted->value = &get_uncasted_ssa_name_node->data;
    add_u64_set(&block->defined_ssa_names, casted_ssa_name.u16);
    put_u64_hash_table(&ui->lox_ir->definitions_by_ssa_name, casted_ssa_name.u16, define_casted);
    add_before_control_node_lox_ir_block(block, control_uses_phi, &define_casted->control);

    //Insert cast node
    insert_cast_node(ui, define_casted->value, (void**) &define_casted->value, type_to_be_casted);

    //control_uses_phi
    remove_u64_set(&phi_node->ssa_versions, ssa_name_to_extract.value.version);
    add_u64_set(&phi_node->ssa_versions, casted_ssa_name.value.version);
    add_ssa_name_use_lox_ir(ui->lox_ir, casted_ssa_name, &define_casted->control);
    remove_ssa_name_use_lox_ir(ui->lox_ir, ssa_name_to_extract, control_uses_phi);
}

static bool is_ssa_name_unboxed(struct ui * ui, struct lox_ir_block * block, struct ssa_name ssa_name) {
    struct lox_ir_type * type = get_type_by_ssa_name_lox_ir(ui->lox_ir, block, ssa_name);
    return type->type == LOX_IR_TYPE_F64 || is_native_lox_ir_type(type->type);
}

static bool control_requires_lox_input(struct ui * ui, struct lox_ir_control_node * control) {
    switch (control->type) {
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT:
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD:
            return is_marked_as_escaped_lox_ir_control(control);
        case LOX_IR_CONTORL_NODE_SET_GLOBAL:
        case LOX_IR_CONTROL_NODE_RETURN:
            return true;
        case LOX_IR_CONTROL_NODE_PRINT:
        case LOX_IR_CONTROL_NODE_DATA:
        case LOX_IR_CONTROL_NODE_ENTER_MONITOR:
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR:
        case LOX_IR_CONTORL_NODE_SET_LOCAL:
        case LOX_IR_CONTROL_NODE_LOOP_JUMP:
        case LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP:
        case LOX_IR_CONTROL_NODE_GUARD:
            return false;
        case LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME: {
            struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) control;
            struct u64_set * uses_ssa_name = get_u64_hash_table(&ui->lox_ir->node_uses_by_ssa_name, define->ssa_name.u16);
            if(uses_ssa_name == NULL){
                return false;
            }

            bool some_use_required_unboxed = false;

            FOR_EACH_U64_SET_VALUE(*uses_ssa_name, node_uses_ssa_name_ptr_u64) {
                struct lox_ir_control_node * node_uses_ssa_name = (struct lox_ir_control_node *) node_uses_ssa_name_ptr_u64;

                if(node_uses_ssa_name->type != LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT &&
                   node_uses_ssa_name->type != LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD &&
                   node_uses_ssa_name->type != LOX_IR_CONTORL_NODE_SET_GLOBAL &&
                   node_uses_ssa_name->type != LOX_IR_CONTROL_NODE_RETURN){
                    some_use_required_unboxed = true;
                    break;
                }
            }

            return !some_use_required_unboxed;
        }
    }
}

static lox_ir_type_t calculate_data_input_type_should_produce(
        struct ui * ui,
        struct lox_ir_block * block,
        struct lox_ir_data_node * data,
        struct lox_ir_data_node * input,
        bool parent_should_produce_lox
) {
    if (data->type != LOX_IR_DATA_NODE_BINARY) {
        return calculate_type_should_produce_data(ui, block, input, parent_should_produce_lox);
    }

    struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) data;
    lox_ir_type_t right_type = binary->right->produced_type->type;
    lox_ir_type_t left_type = binary->left->produced_type->type;
    //A constant can be of type lox and native, since the value can be modified at compile time.
    //That means that if one operator node is const, we will return the type of the other operator
    bool is_right_constant = binary->right->type == LOX_IR_DATA_NODE_CONSTANT;
    bool is_left_constant = binary->left->type == LOX_IR_DATA_NODE_CONSTANT;

    bool binary_produces_string = IS_STRING_LOX_IR_TYPE(binary->data.produced_type->type);
    bool binary_produces_f64 = binary->data.produced_type->type == LOX_IR_TYPE_F64
            || binary->data.produced_type->type == LOX_IR_TYPE_LOX_ANY;
    bool some_f64_format = (right_type == LOX_IR_TYPE_F64 || (right_type == LOX_IR_TYPE_LOX_I64 && !is_right_constant))
                           || (left_type == LOX_IR_TYPE_F64 || (left_type == LOX_IR_TYPE_LOX_I64 && !is_left_constant));
    bool some_any = right_type == LOX_IR_TYPE_LOX_ANY || left_type == LOX_IR_TYPE_LOX_ANY;
    bool both_native = (is_native_lox_ir_type(right_type) || is_right_constant)
            && (is_native_lox_ir_type(left_type) || is_left_constant);

    switch (binary->operator) {
        case OP_ADD: {
            if (binary_produces_string) {
                //When emitting machine code, we will emit a function call to a generic function to concatenate strings
                return parent_should_produce_lox ? LOX_IR_TYPE_LOX_STRING : LOX_IR_TYPE_NATIVE_STRING;
            }
        }
        case OP_SUB:
        case OP_MUL:
        case OP_DIV: {
            if(some_f64_format || binary_produces_f64){
                return LOX_IR_TYPE_F64;
            }
            //Otherwise it produces a native i64
            return parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64;
        }
        case OP_GREATER:
        case OP_EQUAL:
        case OP_LESS: {
            if (some_f64_format || some_any) {
                return LOX_IR_TYPE_F64;
            }
            if (both_native) {
                return parent_should_produce_lox ?
                    native_type_to_lox_ir_type(left_type) :
                    lox_type_to_native_lox_ir_type(left_type);
            }

            lox_ir_type_t produced_type = is_native_lox_ir_type(right_type) ? right_type : left_type;

            return parent_should_produce_lox ?
                   native_type_to_lox_ir_type(left_type) :
                   lox_type_to_native_lox_ir_type(left_type);
        }
        case OP_BINARY_OP_AND:
        case OP_BINARY_OP_OR:
        case OP_LEFT_SHIFT:
        case OP_MODULO:
        case OP_RIGHT_SHIFT:
            return parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64;
    }
}

static lox_ir_type_t calculate_type_should_produce_data(
        struct ui * ui,
        struct lox_ir_block * block,
        struct lox_ir_data_node * data,
        bool parent_should_produce_lox
) {
    switch (data->type) {
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: return parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64;
        case LOX_IR_DATA_NODE_GET_GLOBAL: return LOX_IR_TYPE_LOX_ANY;
        case LOX_IR_DATA_NODE_CALL: return LOX_IR_TYPE_LOX_ANY; //TODO Native funtion calls
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) data;
            lox_ir_type_t type_left = binary->left->produced_type->type;
            lox_ir_type_t type_right = binary->right->produced_type->type;
            bool some_string = type_left == LOX_IR_TYPE_LOX_STRING || type_right == LOX_IR_TYPE_LOX_STRING
                || type_left == LOX_IR_TYPE_NATIVE_STRING || type_right == LOX_IR_TYPE_NATIVE_STRING;

            switch (binary->operator) {
                case OP_ADD:
                    if (some_string) {
                        return parent_should_produce_lox ? LOX_IR_TYPE_LOX_STRING : LOX_IR_TYPE_NATIVE_STRING;
                    }
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                    if (type_left == LOX_IR_TYPE_LOX_I64 && type_right == LOX_IR_TYPE_LOX_I64) {
                        return parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64;
                    } else if (type_left == LOX_IR_TYPE_F64 || type_right == LOX_IR_TYPE_F64) {
                        return LOX_IR_TYPE_F64;
                    } else {
                        return parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64;
                    }
                case OP_GREATER:
                case OP_LESS:
                case OP_EQUAL:
                    return parent_should_produce_lox ? LOX_IR_TYPE_LOX_BOOLEAN : LOX_IR_TYPE_NATIVE_BOOLEAN;
                case OP_BINARY_OP_AND:
                case OP_BINARY_OP_OR:
                case OP_LEFT_SHIFT:
                case OP_MODULO:
                case OP_RIGHT_SHIFT:
                    return parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64;
            }
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) data;
            bool is_f64 = unary->operand->produced_type->type;
            return is_f64 ? LOX_IR_TYPE_F64 : (parent_should_produce_lox ? LOX_IR_TYPE_LOX_I64 : LOX_IR_TYPE_NATIVE_I64);
        }
        case LOX_IR_DATA_NODE_CONSTANT: {
            struct lox_ir_data_constant_node * const_node = (struct lox_ir_data_constant_node *) data;
            return !parent_should_produce_lox ?
                   lox_type_to_native_lox_ir_type(const_node->data.produced_type->type) :
                   const_node->data.produced_type->type;
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD:
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT:
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT:
        case LOX_IR_DATA_NODE_GUARD:
        case LOX_IR_DATA_NODE_PHI:
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            lox_ir_type_t produced_type = data->produced_type->type;
            return parent_should_produce_lox ?
                native_type_to_lox_ir_type(produced_type) :
                lox_type_to_native_lox_ir_type(produced_type);
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            lox_ir_type_t produced_type = is_ssa_name_unboxed(ui, block, GET_USED_SSA_NAME(data)) ?
                   lox_type_to_native_lox_ir_type(data->produced_type->type) :
                   data->produced_type->type;
            return parent_should_produce_lox ?
                   native_type_to_lox_ir_type(produced_type) :
                   lox_type_to_native_lox_ir_type(produced_type);
        }
        default: //TODO Runtime panic
    }
}

static struct ui * alloc_unbox_insertion(struct lox_ir * lox_ir) {
    struct ui * ui =  NATIVE_LOX_MALLOC(sizeof(struct ui));
    ui->lox_ir = lox_ir;
    struct arena arena;
    init_arena(&arena);
    ui->ui_allocator = to_lox_allocator_arena(arena);

    return ui;
}

static void free_unbox_insertion(struct ui * ui) {
    free_arena(&ui->ui_allocator.arena);
    NATIVE_LOX_FREE(ui);
}