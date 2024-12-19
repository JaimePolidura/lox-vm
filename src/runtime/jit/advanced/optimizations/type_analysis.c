#include "type_analysis.h"

struct ta {
    struct u64_hash_table ssa_type_by_ssa_name;
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator ta_allocator;
};

static struct ta * alloc_type_analysis(struct ssa_ir *);
static void free_type_analysis(struct ta *);
static void perform_type_analysis_block(struct ta *, struct ssa_block *);
static void perform_type_analysis_control(struct ta * ta, struct ssa_control_node *);
static bool perform_type_analysis_data(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static struct ssa_type * get_type_data_node_recursive(struct ta *ta, struct ssa_data_node *node);
static void add_argument_types(struct ta *);
static struct ssa_data_node * create_unbox_node(struct ta *ta, struct ssa_data_node *to_unbox);
static struct ssa_data_node * create_box_node(struct ta *ta, struct ssa_data_node *to_box);
static ssa_type_t binary_to_ssa_type(bytecode_t, ssa_type_t, ssa_type_t);
static struct ssa_type * union_type(struct ta *, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_struct_types_same_definition(struct ta*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_array_types(struct ta*, struct ssa_type*, struct ssa_type*);
static void insert_box_node_if_neccesary(struct ta*, struct ssa_control_node *);

extern void runtime_panic(char * format, ...);

void perform_type_analysis(struct ssa_ir * ssa_ir) {
    struct ta * ta = alloc_type_analysis(ssa_ir);

    add_argument_types(ta);

    struct stack_list pending_blocks;
    init_stack_list(&pending_blocks, &ta->ta_allocator.lox_allocator);
    push_stack_list(&pending_blocks, ssa_ir->first_block);
    while(!is_empty_stack_list(&pending_blocks)) {
        struct ssa_block * current_block = pop_stack_list(&pending_blocks);

        perform_type_analysis_block(ta, current_block);

        switch (current_block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ: {
                push_stack_list(&pending_blocks, current_block->next_as.next);
            }
            case TYPE_NEXT_SSA_BLOCK_BRANCH: {
                push_stack_list(&pending_blocks, current_block->next_as.branch.true_branch);
                push_stack_list(&pending_blocks, current_block->next_as.branch.false_branch);
            }
        }
    }

    free_type_analysis(ta);
}

static void perform_type_analysis_block(struct ta * ta, struct ssa_block * block) {
    for(struct ssa_control_node * current_control = block->first;; current_control = current_control->next){
        perform_type_analysis_control(ta, current_control);

        if(current_control == block->last){
            break;
        }
    }
}

static void perform_type_analysis_control(struct ta * ta, struct ssa_control_node * control_node) {
    for_each_data_node_in_control_node(control_node, ta, SSA_DATA_NODE_OPT_PRE_ORDER, perform_type_analysis_data);

    insert_box_node_if_neccesary(ta, control_node);

    if(control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME){
        struct ssa_control_define_ssa_name_node * define = (struct ssa_control_define_ssa_name_node *) control_node;
        put_u64_hash_table(&ta->ssa_type_by_ssa_name, define->ssa_name.u16, define->value->produced_type);
    }
}

static void insert_box_node_if_neccesary(struct ta * ta, struct ssa_control_node * control_node) {
    switch (control_node->type) {
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) control_node;
            return_node->data = create_box_node(ta, return_node->data);
            return_node->data->produced_type->type = native_type_to_lox_ssa_type(return_node->data->produced_type->type);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) control_node;
            print_node->data = create_box_node(ta, print_node->data);
            print_node->data->produced_type->type = native_type_to_lox_ssa_type(print_node->data->produced_type->type);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
            set_global->value_node = create_box_node(ta, set_global->value_node);
            set_global->value_node->produced_type->type = native_type_to_lox_ssa_type(set_global->value_node->produced_type->type);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
            set_struct_field->new_field_value = create_box_node(ta, set_struct_field->new_field_value);
            set_struct_field->new_field_value->produced_type->type = native_type_to_lox_ssa_type(set_struct_field->new_field_value->produced_type->type);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) control_node;
            set_array_element->new_element_value = create_box_node(ta, set_array_element->new_element_value);
            set_array_element->new_element_value->produced_type->type = native_type_to_lox_ssa_type(set_array_element->new_element_value->produced_type->type);
            break;
        }
        default: break;
    }
}

static bool perform_type_analysis_data(
        struct ssa_data_node * _,
        void ** __,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct ta * ta = extra;
    data_node->produced_type = get_type_data_node_recursive(ta, data_node);
    return false;
}

static struct ssa_type * get_type_data_node_recursive(struct ta * ta, struct ssa_data_node * node) {
    switch (node->type) {
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant = (struct ssa_data_constant_node *) node;
            profile_data_type_t type = lox_value_to_profile_type(constant->constant_lox_value);
            return CREATE_SSA_TYPE(profiled_type_to_ssa_type(type), SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            return CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            return CREATE_SSA_TYPE(SSA_TYPE_LOX_I64, SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            struct ssa_type * unary_operand_type = get_type_data_node_recursive(ta, unary->operand);
            unary->operand->produced_type = unary_operand_type;
            return unary_operand_type;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            return get_u64_hash_table(&ta->ssa_type_by_ssa_name, get_ssa_name->ssa_name.u16);
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call = (struct ssa_data_function_call_node *) node;
            for(int i = 0; i < call->n_arguments; i++){
                call->arguments[i]->produced_type = get_type_data_node_recursive(ta, call->arguments[i]);
            }
            profile_data_type_t returned_type;
            if(!call->is_native) {
                struct instruction_profile_data instruction_profile_data = get_instruction_profile_data_function(
                        ta->ssa_ir->function, node->original_bytecode);
                struct function_call_profile function_call_profile = instruction_profile_data.as.function_call;
                returned_type = get_type_by_type_profile_data(function_call_profile.returned_type_profile);
            } else {
                returned_type = call->native_function->returned_type;
            }

            return CREATE_SSA_TYPE(profiled_type_to_ssa_type(returned_type), SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            struct ssa_type * right_type = get_type_data_node_recursive(ta, binary->right);
            struct ssa_type * left_type = get_type_data_node_recursive(ta, binary->left);
            bool some_any = right_type->type == SSA_TYPE_LOX_ANY || left_type->type == SSA_TYPE_LOX_ANY;

            if (!some_any && IS_LOX_SSA_TYPE(right_type->type)) {
                right_type = CREATE_SSA_TYPE(lox_type_to_native_ssa_type(right_type->type), SSA_IR_ALLOCATOR(ta->ssa_ir));
                binary->right = create_unbox_node(ta, binary->right);
            }
            if (!some_any && IS_LOX_SSA_TYPE(left_type->type)) {
                left_type = CREATE_SSA_TYPE(lox_type_to_native_ssa_type(left_type->type), SSA_IR_ALLOCATOR(ta->ssa_ir));
                binary->left = create_unbox_node(ta, binary->left);
            }
            if (some_any && IS_NATIVE_SSA_TYPE(left_type->type)) {
                left_type = CREATE_SSA_TYPE(native_type_to_lox_ssa_type(left_type->type), SSA_IR_ALLOCATOR(ta->ssa_ir));
                binary->left = create_box_node(ta, binary->left);
            }
            if (some_any && IS_NATIVE_SSA_TYPE(right_type->type)) {
                right_type = CREATE_SSA_TYPE(native_type_to_lox_ssa_type(right_type->type), SSA_IR_ALLOCATOR(ta->ssa_ir));
                binary->right = create_box_node(ta, binary->right);
            }

            binary->right->produced_type = right_type;
            binary->left->produced_type = left_type;
            ssa_type_t produced_type = binary_to_ssa_type(binary->operator, left_type->type, right_type->type);

            return CREATE_SSA_TYPE(produced_type, SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_data_guard_node * guard = (struct ssa_data_guard_node *) node;
            switch (guard->guard.type) {
                case SSA_GUARD_TYPE_CHECK: {
                    profile_data_type_t guard_type = guard->guard.value_to_compare.type;
                    return CREATE_SSA_TYPE(profiled_type_to_ssa_type(guard_type), SSA_IR_ALLOCATOR(ta->ssa_ir));
                }
                case SSA_GUARD_STRUCT_DEFINITION_TYPE_CHECK: {
                    return get_type_data_node_recursive(ta, guard->guard.value);
                }
                case SSA_GUARD_BOOLEAN_CHECK: {
                    return CREATE_SSA_TYPE(SSA_TYPE_NATIVE_BOOLEAN, SSA_IR_ALLOCATOR(ta->ssa_ir));
                }
            }
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) node;
            struct ssa_type * ssa_type = CREATE_STRUCT_SSA_TYPE(init_struct->definition, SSA_IR_ALLOCATOR(ta->ssa_ir));

            for(int i = 0; i < init_struct->definition->n_fields; i++) {
                struct ssa_data_node * field_node_arg = init_struct->fields_nodes[i];
                struct ssa_type * field_node_type = get_type_data_node_recursive(ta, field_node_arg);
                field_node_arg->produced_type = field_node_type;
                struct string_object * field_name = init_struct->definition->field_names[i];

                put_u64_hash_table(&ssa_type->value.struct_instance->type_by_field_name, (uint64_t) field_name->chars,
                                   field_node_type);
            }

            return ssa_type;
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) node;
            struct ssa_type * phi_type_result = NULL;

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_ssa_name) {
                struct ssa_type * type_current_ssa_name = get_u64_hash_table(&ta->ssa_type_by_ssa_name, current_ssa_name.u16);

                if (phi_type_result == NULL) {
                    phi_type_result = type_current_ssa_name;
                } else {
                    phi_type_result = union_type(ta, phi_type_result, type_current_ssa_name);
                }

                return phi_type_result;
            }
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) node;
            int array_size = init_array->n_elements;
            struct ssa_type * type = NULL;

            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                struct ssa_data_node * array_element_type = init_array->elememnts_node[i];
                array_element_type->produced_type = get_type_data_node_recursive(ta, array_element_type);

                if(type == NULL){
                    type = array_element_type->produced_type;
                } else if(type->type != SSA_TYPE_LOX_ANY){
                    type = !is_eq_ssa_type(type, array_element_type->produced_type) ?
                           CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir)):
                           array_element_type->produced_type;
                }
            }

            return CREATE_ARRAY_SSA_TYPE(array_size, type, SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            struct ssa_data_node * instance_node = get_struct_field->instance_node;
            struct ssa_type * type_struct_field = get_type_data_node_recursive(ta, instance_node);
            instance_node->produced_type = type_struct_field;

            if(IS_STRUCT_INSTANCE_SSA_TYPE(type_struct_field->type)){
                return get_struct_field_ssa_type(
                        type_struct_field, get_struct_field->field_name->chars,SSA_IR_ALLOCATOR(ta->ssa_ir)
                );
            } else {
                return CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir));
            }
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) node;
            struct ssa_data_node * array_node = get_array_element->instance;
            struct ssa_data_node * index_node = get_array_element->index;
            struct ssa_type * type_array = get_type_data_node_recursive(ta, array_node);
            struct ssa_type * type_index = get_type_data_node_recursive(ta, index_node);
            array_node->produced_type = type_array;
            index_node->produced_type = type_index;

            if(IS_STRUCT_INSTANCE_SSA_TYPE(type_array->type)){
                return type_array;
            } else {
                return CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir));
            }
        }

        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            runtime_panic("Illegal code path");
        }
    }
}

//This function will insert box/unbox nodes in the definitions of the ssa node
static struct ssa_type * union_type(
        struct ta * ta,
        struct ssa_type * a,
        struct ssa_type * b
) {
    if(a->type == b->type){
        //Different struct instances
        if(IS_STRUCT_INSTANCE_SSA_TYPE(a->type) &&
            a->value.struct_instance->definition == b->value.struct_instance->definition &&
            a->value.struct_instance != b->value.struct_instance) {
            return union_struct_types_same_definition(ta, a, b);

        } else if(IS_STRUCT_INSTANCE_SSA_TYPE(a->type) &&
                  a->value.struct_instance->definition != b->value.struct_instance->definition){
            return CREATE_STRUCT_SSA_TYPE(NULL, SSA_IR_ALLOCATOR(ta->ssa_ir));

        } else if(IS_ARRAY_SSA_TYPE(a->type) &&
            a->value.array != b->value.array) {
            return union_array_types(ta, a, b);
        } else {
            return a;
        }
    } else {
        //TODO
    }
}

static struct ta * alloc_type_analysis(struct ssa_ir * ssa_ir) {
    struct ta * ta =  NATIVE_LOX_MALLOC(sizeof(struct ta));
    ta->ssa_ir = ssa_ir;
    struct arena arena;
    init_arena(&arena);
    ta->ta_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&ta->ssa_type_by_ssa_name, &ta->ta_allocator.lox_allocator);
    return ta;
}

static void free_type_analysis(struct ta * ta) {
    free_arena(&ta->ta_allocator.arena);
    NATIVE_LOX_FREE(ta);
}

static void add_argument_types(struct ta * ta) {
    struct function_object * function = ta->ssa_ir->function;

    for(int i = 1; i < ta->ssa_ir->function->n_arguments + 1; i++){
        profile_data_type_t profiled_type = get_function_argument_profiled_type(function, i);
        struct ssa_name function_arg_ssa_name = CREATE_SSA_NAME(i, 0);
        struct ssa_type * function_arg_type = CREATE_SSA_TYPE(profiled_type_to_ssa_type(profiled_type),
                SSA_IR_ALLOCATOR(ta->ssa_ir));

        put_u64_hash_table(&ta->ssa_type_by_ssa_name, function_arg_ssa_name.u16, function_arg_type);
    }
}

static struct ssa_data_node * create_unbox_node(struct ta * ta, struct ssa_data_node * to_unbox) {
    if(to_unbox->type != SSA_DATA_NODE_TYPE_UNBOX){
        struct ssa_data_node_unbox * unbox_node = ALLOC_SSA_DATA_NODE(
                SSA_DATA_NODE_TYPE_UNBOX, struct ssa_data_node_unbox, NULL, SSA_IR_ALLOCATOR(ta->ssa_ir)
        );
        unbox_node->to_unbox = to_unbox;
        return &unbox_node->data;
    } else {
        return to_unbox;
    }
}

static struct ssa_data_node * create_box_node(struct ta * ta, struct ssa_data_node * to_box) {
    if(to_box->type != SSA_DATA_NODE_TYPE_BOX){
        struct ssa_data_node_box * box_node = ALLOC_SSA_DATA_NODE(
                SSA_DATA_NODE_TYPE_BOX, struct ssa_data_node_box, NULL, SSA_IR_ALLOCATOR(ta->ssa_ir)
        );
        box_node->to_box = to_box;
        return &box_node->data;
    } else {
        return to_box;
    }
}

//Expect left & right to be both of them lox or native
ssa_type_t binary_to_ssa_type(bytecode_t operator, ssa_type_t left, ssa_type_t right) {
    if(left == right){
        return left;
    }

    bool is_native = IS_NATIVE_SSA_TYPE(left);

    switch (operator) {
        case OP_GREATER:
        case OP_EQUAL:
        case OP_LESS: {
            return is_native ? SSA_TYPE_NATIVE_BOOLEAN : SSA_TYPE_LOX_BOOLEAN;
        }
        case OP_BINARY_OP_AND:
        case OP_BINARY_OP_OR:
        case OP_LEFT_SHIFT:
        case OP_RIGHT_SHIFT:
        case OP_MODULO: {
            return is_native ? SSA_TYPE_NATIVE_I64 : SSA_TYPE_LOX_I64;
        }
        case OP_ADD: {
            if(IS_STRING_SSA_TYPE(left) || IS_STRING_SSA_TYPE(right)){
                return is_native ? SSA_TYPE_NATIVE_STRING : SSA_TYPE_LOX_STRING;
            }
        }
        case OP_SUB:
        case OP_MUL: {
            if(IS_F64_SSA_TYPE(left) || IS_F64_SSA_TYPE(right)){
                return SSA_TYPE_F64;
            } else {
                return is_native ? SSA_TYPE_NATIVE_I64 : SSA_TYPE_LOX_I64;
            }
        }
        case OP_DIV: {
            return SSA_TYPE_F64;
        }
    }
}

//Expect same definition
static struct ssa_type * union_struct_types_same_definition(
        struct ta * ta,
        struct ssa_type * a,
        struct ssa_type * b
) {
    struct struct_definition_object * definition = a->value.struct_instance->definition;
    struct ssa_type * union_struct = CREATE_STRUCT_SSA_TYPE(definition, SSA_IR_ALLOCATOR(ta->ssa_ir));
    struct struct_instance_ssa_type * struct_instance_ssa_type = union_struct->value.struct_instance;

    for(int i = 0; definition != NULL && i < definition->n_fields; i++){
        char * field_name = definition->field_names[i]->chars;
        struct ssa_type * field_a_type = get_u64_hash_table(&a->value.struct_instance->type_by_field_name, (uint64_t) field_name);
        struct ssa_type * field_b_type = get_u64_hash_table(&b->value.struct_instance->type_by_field_name, (uint64_t) field_name);

        struct ssa_type * new_field_type = field_a_type->type != field_b_type->type ?
                CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir)) :
                union_type(ta, field_a_type, field_b_type);

        put_u64_hash_table(&struct_instance_ssa_type->type_by_field_name, (uint64_t) field_name, new_field_type);
    }

    return union_struct;
}

static struct ssa_type * union_array_types(
        struct ta * ta,
        struct ssa_type * a,
        struct ssa_type * b
) {
    //This might be usefult for range check elimination
    int new_array_size = MIN(a->value.array->size, b->value.array->size);
    struct ssa_type * new_array_type = !is_eq_ssa_type(a->value.array->type, b->value.array->type) ?
            CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir)) :
            a->value.array->type;

    return CREATE_ARRAY_SSA_TYPE(new_array_size, new_array_type, SSA_IR_ALLOCATOR(ta->ssa_ir));
}