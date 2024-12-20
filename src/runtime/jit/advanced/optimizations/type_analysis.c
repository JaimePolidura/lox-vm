#include "type_analysis.h"

struct ta {
    struct u64_hash_table ssa_type_by_ssa_name;
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator ta_allocator;
};

static struct ta * alloc_type_analysis(struct ssa_ir*);
static void free_type_analysis(struct ta*);
static void perform_type_analysis_block(struct ta*, struct ssa_block *);
static void perform_type_analysis_control(struct ta* ta, struct ssa_control_node *);
static bool perform_type_analysis_data(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static struct ssa_type * get_type_data_node_recursive(struct ta*, struct ssa_data_node*, struct ssa_data_node** parent_ptr);
static void add_argument_types(struct ta*);
static ssa_type_t binary_to_ssa_type(bytecode_t, ssa_type_t, ssa_type_t);
static struct ssa_type * union_type(struct ta*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_struct_types_same_definition(struct ta*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_array_types(struct ta*, struct ssa_type*, struct ssa_type*);
static void clear_type(struct ssa_type *);

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

    if(control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME){
        struct ssa_control_define_ssa_name_node * define = (struct ssa_control_define_ssa_name_node *) control_node;
        put_u64_hash_table(&ta->ssa_type_by_ssa_name, define->ssa_name.u16, define->value->produced_type);

    } else if (control_node->type == SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD) {
        struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) control_node;
        if(set_struct_field->instance->produced_type->type == SSA_TYPE_LOX_STRUCT_INSTANCE) {
            struct struct_instance_ssa_type * struct_instance_type = set_struct_field->instance->produced_type->value.struct_instance;
            put_u64_hash_table(&struct_instance_type->type_by_field_name, (uint64_t) set_struct_field->field_name->chars,
                               set_struct_field->new_field_value->produced_type);
        }
    } else if(control_node->type == SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT) {
        struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) control_node;
        if(set_array_element->array->produced_type->type == SSA_TYPE_LOX_ARRAY) {
            struct array_ssa_type * array_type = set_array_element->array->produced_type->value.array;
            array_type->type = union_type(ta, array_type->type, set_array_element->new_element_value->produced_type);
        }
    } else if (control_node->type == SSA_CONTORL_NODE_TYPE_SET_GLOBAL) {
        struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
        clear_type(set_global->value_node->produced_type);
    }
}

static bool perform_type_analysis_data(
        struct ssa_data_node * _,
        void ** parent_ptr,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct ta * ta = extra;
    data_node->produced_type = get_type_data_node_recursive(ta, data_node, (struct ssa_data_node **) parent_ptr);
    return false;
}

static struct ssa_type * get_type_data_node_recursive(
        struct ta * ta,
        struct ssa_data_node * node,
        struct ssa_data_node ** parent_ptr
) {
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
            struct ssa_type * unary_operand_type = get_type_data_node_recursive(ta, unary->operand, &unary->operand);
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
                struct ssa_data_node * argument = call->arguments[i];
                argument->produced_type = get_type_data_node_recursive(ta, argument, &call->arguments[i]);
                //Functions might modify the array/struct. So we need to clear its type
                clear_type(argument->produced_type);
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
            struct ssa_type * right_type = get_type_data_node_recursive(ta, binary->right, &binary->right);
            struct ssa_type * left_type = get_type_data_node_recursive(ta, binary->left, &binary->left);

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
                    struct struct_definition_object * definition = guard->guard.value_to_compare.struct_definition;
                    return CREATE_STRUCT_SSA_TYPE(definition, SSA_IR_ALLOCATOR(ta->ssa_ir));
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
                struct ssa_type * field_node_type = get_type_data_node_recursive(ta, field_node_arg, &init_struct->fields_nodes[i]);
                field_node_arg->produced_type = field_node_type;
                struct string_object * field_name = init_struct->definition->field_names[i];

                put_u64_hash_table(&ssa_type->value.struct_instance->type_by_field_name, (uint64_t) field_name->chars,
                                   field_node_type);
            }

            return ssa_type;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * init_array = (struct ssa_data_initialize_array_node *) node;
            struct ssa_type * type = NULL;

            if(init_array->empty_initialization){
                type = CREATE_SSA_TYPE(SSA_TYPE_UNKNOWN, SSA_IR_ALLOCATOR(ta->ssa_ir));
            }

            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                struct ssa_data_node * array_element_type = init_array->elememnts_node[i];
                array_element_type->produced_type = get_type_data_node_recursive(ta, array_element_type, &init_array->elememnts_node[i]);

                if(type == NULL){
                    type = array_element_type->produced_type;
                } else if(type->type != SSA_TYPE_LOX_ANY){
                    type = !is_eq_ssa_type(type, array_element_type->produced_type) ?
                           CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir)):
                           array_element_type->produced_type;
                }
            }

            return CREATE_ARRAY_SSA_TYPE(type, SSA_IR_ALLOCATOR(ta->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            struct ssa_data_node * instance_node = get_struct_field->instance_node;
            struct instruction_profile_data get_struct_field_profile_data = get_instruction_profile_data_function(
                    ta->ssa_ir->function, get_struct_field->data.original_bytecode);
            struct type_profile_data get_struct_field_type_profile = get_struct_field_profile_data.as.struct_field.get_struct_field_profile;

            struct ssa_type * type_struct_instance = get_type_data_node_recursive(ta, instance_node, &get_struct_field->instance_node);

            //We insert a struct definition guard
            if (type_struct_instance->type != SSA_TYPE_LOX_STRUCT_INSTANCE) {
                struct ssa_data_guard_node * struct_instance_guard = create_from_profile_ssa_data_guard_node(get_struct_field_type_profile,
                        get_struct_field->instance_node, SSA_IR_ALLOCATOR(ta->ssa_ir));
                get_struct_field->instance_node = &struct_instance_guard->data;
                type_struct_instance = get_type_data_node_recursive(ta, instance_node, &get_struct_field->instance_node);
                instance_node->produced_type = type_struct_instance;
            }

            struct ssa_type * field_type = get_u64_hash_table(
                    &instance_node->produced_type->value.struct_instance->type_by_field_name,
                    (uint64_t) get_struct_field->field_name->chars
            );

            //Field type found
            if (field_type != NULL) {
                instance_node->produced_type = type_struct_instance;
                return field_type;
            }

            //If not found, we insert a type guard in the get_struct_field
            struct ssa_data_guard_node * guard_node = create_from_profile_ssa_data_guard_node(get_struct_field_type_profile,
                    &get_struct_field->data, SSA_IR_ALLOCATOR(ta->ssa_ir));
            struct ssa_type * type_struct_field = get_type_data_node_recursive(ta, &guard_node->data, &guard_node->guard.value);
            *parent_ptr = &guard_node->data;

            return type_struct_field;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) node;
            struct instruction_profile_data get_struct_field_profile_data = get_instruction_profile_data_function(
                    ta->ssa_ir->function, get_array_element->data.original_bytecode);
            struct type_profile_data get_struct_field_type_profile = get_struct_field_profile_data.as.struct_field.get_struct_field_profile;

            struct ssa_data_node * array_instance = get_array_element->instance;
            struct ssa_data_node * index = get_array_element->index;
            array_instance->produced_type = get_type_data_node_recursive(ta, array_instance, &get_array_element->instance);
            index->produced_type = get_type_data_node_recursive(ta, index, &get_array_element->index);

            //We insert array type check guard
            if(array_instance->produced_type->type != SSA_TYPE_LOX_ARRAY){
                struct ssa_data_guard_node * array_type_guard = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_GUARD,
                        struct ssa_data_guard_node, NULL, SSA_IR_ALLOCATOR(ta->ssa_ir));
                array_type_guard->guard.value_to_compare.type = PROFILE_DATA_TYPE_ARRAY;
                array_type_guard->guard.type = SSA_GUARD_TYPE_CHECK;
                array_type_guard->guard.value = array_instance;
                get_array_element->instance = &array_type_guard->data;
                array_instance->produced_type = get_type_data_node_recursive(ta, &array_type_guard->data, &array_type_guard->guard.value);
            }

            //Array type found
            if(array_instance->produced_type->value.array->type->type != SSA_TYPE_UNKNOWN &&
               array_instance->produced_type->value.array->type->type != SSA_TYPE_LOX_ANY){
                return array_instance->produced_type->value.array->type;
            }

            //If not found, add guard type check
            struct ssa_data_guard_node * array_element_type_guard = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_GUARD,
                    struct ssa_data_guard_node, NULL, SSA_IR_ALLOCATOR(ta->ssa_ir));
            array_element_type_guard->guard.value_to_compare.type = get_type_by_type_profile_data(get_struct_field_type_profile);
            array_element_type_guard->guard.value = &get_array_element->data;
            array_element_type_guard->guard.type = SSA_GUARD_TYPE_CHECK;
            *parent_ptr = &array_element_type_guard->data;

            return get_type_data_node_recursive(ta, &array_element_type_guard->data, parent_ptr);
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

        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            runtime_panic("Illegal code path");
        }
    }
}

static struct ssa_type * union_type(
        struct ta * ta,
        struct ssa_type * a,
        struct ssa_type * b
) {
    if(a->type == b->type){
        //Different struct instances
        if(a->type == SSA_TYPE_LOX_STRUCT_INSTANCE &&
            a->value.struct_instance->definition == b->value.struct_instance->definition &&
            a->value.struct_instance != b->value.struct_instance) {
            return union_struct_types_same_definition(ta, a, b);

        } else if(a->type == SSA_TYPE_LOX_STRUCT_INSTANCE &&
                  a->value.struct_instance->definition != b->value.struct_instance->definition){
            return CREATE_STRUCT_SSA_TYPE(NULL, SSA_IR_ALLOCATOR(ta->ssa_ir));

        } else if(a->type == SSA_TYPE_LOX_ARRAY &&
            a->value.array->type->type != b->value.array->type->type) {
            return union_array_types(ta, a, b);
        } else {
            return a;
        }
    } else {
        return CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir));
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

    for (int i = 1; i < ta->ssa_ir->function->n_arguments + 1; i++) {
        struct type_profile_data function_arg_profile_data = get_function_argument_profiled_type(function, i);
        profile_data_type_t function_arg_profiled_type = get_type_by_type_profile_data(function_arg_profile_data);

        struct ssa_name function_arg_ssa_name = CREATE_SSA_NAME(i, 0);
        struct ssa_type * function_arg_type = NULL;

        //TODO Array
        if (function_arg_profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && !function_arg_profile_data.invalid_struct_definition) {
            function_arg_type = CREATE_STRUCT_SSA_TYPE(function_arg_profile_data.struct_definition, SSA_IR_ALLOCATOR(ta->ssa_ir));
        } else {
            function_arg_type = CREATE_SSA_TYPE(profiled_type_to_ssa_type(function_arg_profiled_type), SSA_IR_ALLOCATOR(ta->ssa_ir));
        }

        put_u64_hash_table(&ta->ssa_type_by_ssa_name, function_arg_ssa_name.u16, function_arg_type);
    }
}

//Expect left & right to be both of them lox or native
ssa_type_t binary_to_ssa_type(bytecode_t operator, ssa_type_t left, ssa_type_t right) {
    if(left == right){
        return left;
    }
    if(left == SSA_TYPE_LOX_ANY || right == SSA_TYPE_LOX_ANY){
        return SSA_TYPE_LOX_ANY;
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
            if(left == SSA_TYPE_F64 || right == SSA_TYPE_F64){
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
    struct ssa_type * new_array_type = !is_eq_ssa_type(a->value.array->type, b->value.array->type) ?
            CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(ta->ssa_ir)) :
            a->value.array->type;

    return CREATE_ARRAY_SSA_TYPE(new_array_type, SSA_IR_ALLOCATOR(ta->ssa_ir));
}

static void clear_type(struct ssa_type *type) {
    if(type->type == SSA_TYPE_LOX_ARRAY){
        type->value.array->type = NULL;
    } else if(type->type == SSA_TYPE_LOX_STRUCT_INSTANCE){
        clear_u64_hash_table(&type->value.struct_instance->type_by_field_name);
    }
}