#include "type_analysis.h"

struct ta {
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator ta_allocator;
    //Key block pointer: value pointer to pending_evaluate's ssa_type_by_ssa_name
    struct u64_hash_table ssa_type_by_ssa_name_by_block;

    struct u64_set loops_aready_scanned;
};

struct pending_evaluate {
    struct ta * ta;
    struct u64_hash_table * ssa_type_by_ssa_name;
    struct ssa_block * block;
};

static struct ta * alloc_type_analysis(struct ssa_ir*);
static void free_type_analysis(struct ta*);
static void perform_type_analysis_block(struct pending_evaluate*);
static void perform_type_analysis_control(struct pending_evaluate*, struct ssa_control_node*);
static bool perform_type_analysis_data(struct ssa_data_node*, void**, struct ssa_data_node*, void*);
static struct ssa_type * get_type_data_node_recursive(struct pending_evaluate*, struct ssa_data_node*, struct ssa_data_node** parent_ptr);
static void add_argument_types(struct ta*, struct u64_hash_table*);
static ssa_type_t binary_to_ssa_type(bytecode_t, ssa_type_t, ssa_type_t);
static struct ssa_type * union_type(struct ta*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_struct_types_same_definition(struct ta*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_array_types(struct ta*, struct ssa_type*, struct ssa_type*);
static void clear_type(struct ssa_type *);
static void push_pending_evaluate(struct stack_list*, struct ta*, struct ssa_block*, struct u64_hash_table*);
static struct ssa_type * get_type_by_ssa_name(struct ta*, struct pending_evaluate*, struct ssa_name);
static struct u64_hash_table * get_merged_type_map_block(struct ta *ta, struct ssa_block *next_block);
static struct u64_hash_table * get_ssa_type_by_ssa_name_by_block(struct ta *, struct ssa_block *);

extern void runtime_panic(char * format, ...);

void perform_type_analysis(struct ssa_ir * ssa_ir) {
    struct ta * ta = alloc_type_analysis(ssa_ir);
    struct u64_hash_table * ssa_type_by_ssa_name = LOX_MALLOC(&ta->ta_allocator.lox_allocator, sizeof(struct u64_hash_table));
    init_u64_hash_table(ssa_type_by_ssa_name, &ta->ta_allocator.lox_allocator);

    add_argument_types(ta, ssa_type_by_ssa_name);

    struct stack_list pending_to_evaluate;
    init_stack_list(&pending_to_evaluate, &ta->ta_allocator.lox_allocator);
    push_pending_evaluate(&pending_to_evaluate, ta, ssa_ir->first_block, ssa_type_by_ssa_name);
    put_u64_hash_table(&ta->ssa_type_by_ssa_name_by_block, (uint64_t) ssa_ir->first_block, ssa_type_by_ssa_name);

    while(!is_empty_stack_list(&pending_to_evaluate)) {
        struct pending_evaluate * to_evalute = pop_stack_list(&pending_to_evaluate);
        struct ssa_block * block_to_evalute = to_evalute->block;

        perform_type_analysis_block(to_evalute);

        switch (to_evalute->block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ: {
                struct u64_hash_table * next_block_types = get_merged_type_map_block(to_evalute->ta,
                        block_to_evalute->next_as.next);
                push_pending_evaluate(&pending_to_evaluate, ta, block_to_evalute->next_as.next, next_block_types);
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_BRANCH: {
                struct ssa_block * false_branch = block_to_evalute->next_as.branch.false_branch;
                struct ssa_block * true_branch = block_to_evalute->next_as.branch.true_branch;
                struct u64_hash_table * types_false = get_merged_type_map_block(to_evalute->ta,
                        block_to_evalute->next_as.branch.false_branch);
                struct u64_hash_table * types_true = get_merged_type_map_block(to_evalute->ta,
                        block_to_evalute->next_as.branch.true_branch);

                push_pending_evaluate(&pending_to_evaluate, ta, false_branch, types_false);
                push_pending_evaluate(&pending_to_evaluate, ta, true_branch, types_true);
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_LOOP: {
                struct ssa_block * to_jump_loop_block = block_to_evalute->next_as.loop;
                if (!contains_u64_set(&ta->loops_aready_scanned, (uint64_t) to_jump_loop_block)) {
                    struct u64_hash_table * types_loop = clone_u64_hash_table(to_evalute->ssa_type_by_ssa_name, &ta->ta_allocator.lox_allocator);
                    push_pending_evaluate(&pending_to_evaluate, ta, to_jump_loop_block, types_loop);
                    add_u64_set(&ta->loops_aready_scanned, (uint64_t) to_jump_loop_block);
                }
                break;
            }
        }
    }

    free_type_analysis(ta);
}

static void perform_type_analysis_block(struct pending_evaluate * to_evalute) {
    for(struct ssa_control_node * current_control = to_evalute->block->first;; current_control = current_control->next){
        perform_type_analysis_control(to_evalute, current_control);

        if(current_control == to_evalute->block->last){
            break;
        }
    }
}

static void perform_type_analysis_control(struct pending_evaluate * to_evalute, struct ssa_control_node * control_node) {
    for_each_data_node_in_control_node(control_node, to_evalute, SSA_DATA_NODE_OPT_PRE_ORDER, perform_type_analysis_data);

    if(control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME){
        struct ssa_control_define_ssa_name_node * define = (struct ssa_control_define_ssa_name_node *) control_node;
        put_u64_hash_table(to_evalute->ssa_type_by_ssa_name, define->ssa_name.u16, define->value->produced_type);

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
            array_type->type = union_type(to_evalute->ta, array_type->type, set_array_element->new_element_value->produced_type);
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
    struct pending_evaluate * pending_evaluate = extra;
    data_node->produced_type = get_type_data_node_recursive(pending_evaluate, data_node, (struct ssa_data_node **) parent_ptr);
    return false;
}

static struct ssa_type * get_type_data_node_recursive(
        struct pending_evaluate * to_evaluate,
        struct ssa_data_node * node,
        struct ssa_data_node ** parent_ptr
) {
    struct ta * ta = to_evaluate->ta;

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
            struct ssa_type * unary_operand_type = get_type_data_node_recursive(to_evaluate, unary->operand, &unary->operand);
            unary->operand->produced_type = unary_operand_type;
            return unary_operand_type;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            return get_type_by_ssa_name(ta, to_evaluate, get_ssa_name->ssa_name);
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call = (struct ssa_data_function_call_node *) node;
            for(int i = 0; i < call->n_arguments; i++){
                struct ssa_data_node * argument = call->arguments[i];
                argument->produced_type = get_type_data_node_recursive(to_evaluate, argument, &call->arguments[i]);
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
            struct ssa_type * right_type = get_type_data_node_recursive(to_evaluate, binary->right, &binary->right);
            struct ssa_type * left_type = get_type_data_node_recursive(to_evaluate, binary->left, &binary->left);

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
                struct ssa_type * field_node_type = get_type_data_node_recursive(to_evaluate, field_node_arg, &init_struct->fields_nodes[i]);
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
                array_element_type->produced_type = get_type_data_node_recursive(to_evaluate, array_element_type, &init_array->elememnts_node[i]);

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

            struct ssa_type * type_struct_instance = get_type_data_node_recursive(to_evaluate, instance_node, &get_struct_field->instance_node);

            //We insert a struct definition guard
            if (type_struct_instance->type != SSA_TYPE_LOX_STRUCT_INSTANCE) {
                struct ssa_data_guard_node * struct_instance_guard = create_from_profile_ssa_data_guard_node(get_struct_field_type_profile,
                        get_struct_field->instance_node, SSA_IR_ALLOCATOR(ta->ssa_ir));
                get_struct_field->instance_node = &struct_instance_guard->data;
                type_struct_instance = get_type_data_node_recursive(to_evaluate, instance_node, &get_struct_field->instance_node);
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
            struct ssa_type * type_struct_field = get_type_data_node_recursive(to_evaluate, &guard_node->data, &guard_node->guard.value);
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
            array_instance->produced_type = get_type_data_node_recursive(to_evaluate, array_instance, &get_array_element->instance);
            index->produced_type = get_type_data_node_recursive(to_evaluate, index, &get_array_element->index);

            //We insert array type check guard
            if(array_instance->produced_type->type != SSA_TYPE_LOX_ARRAY){
                struct ssa_data_guard_node * array_type_guard = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_GUARD,
                        struct ssa_data_guard_node, NULL, SSA_IR_ALLOCATOR(ta->ssa_ir));
                array_type_guard->guard.value_to_compare.type = PROFILE_DATA_TYPE_ARRAY;
                array_type_guard->guard.type = SSA_GUARD_TYPE_CHECK;
                array_type_guard->guard.value = array_instance;
                get_array_element->instance = &array_type_guard->data;
                array_instance->produced_type = get_type_data_node_recursive(to_evaluate, &array_type_guard->data, &array_type_guard->guard.value);
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

            return get_type_data_node_recursive(to_evaluate, &array_element_type_guard->data, parent_ptr);
        }

        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) node;
            struct ssa_type * phi_type_result = NULL;

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_ssa_name) {
                struct ssa_type * type_current_ssa_name = get_type_by_ssa_name(ta, to_evaluate, current_ssa_name);

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
    if(a->type == SSA_TYPE_UNKNOWN || b->type == SSA_TYPE_UNKNOWN){
        return CREATE_SSA_TYPE(SSA_TYPE_UNKNOWN, SSA_IR_ALLOCATOR(ta->ssa_ir));
    } else if(a->type == b->type){
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
    init_u64_hash_table(&ta->ssa_type_by_ssa_name_by_block, &ta->ta_allocator.lox_allocator);
    init_u64_set(&ta->loops_aready_scanned, &ta->ta_allocator.lox_allocator);

    return ta;
}

static void free_type_analysis(struct ta * ta) {
    free_arena(&ta->ta_allocator.arena);
    NATIVE_LOX_FREE(ta);
}

static void add_argument_types(struct ta * ta, struct u64_hash_table * ssa_type_by_ssa_name) {
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

        put_u64_hash_table(ssa_type_by_ssa_name, function_arg_ssa_name.u16, function_arg_type);
    }
}

//Expect left & right to be both of them lox or native
ssa_type_t binary_to_ssa_type(bytecode_t operator, ssa_type_t left, ssa_type_t right) {
    if(left == right){
        return left;
    }
    if(left == SSA_TYPE_UNKNOWN || right == SSA_TYPE_UNKNOWN){
        return SSA_TYPE_UNKNOWN;
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

static void push_pending_evaluate(
        struct stack_list * pending_stack,
        struct ta * ta,
        struct ssa_block * block,
        struct u64_hash_table * ssa_type_by_ssa_name
) {
    struct pending_evaluate * pending_evaluate = LOX_MALLOC(&ta->ta_allocator.lox_allocator, sizeof(struct pending_evaluate));
    pending_evaluate->ssa_type_by_ssa_name = ssa_type_by_ssa_name;
    pending_evaluate->block = block;
    pending_evaluate->ta = ta;
    push_stack_list(pending_stack, pending_evaluate);
}

static struct ssa_type * get_type_by_ssa_name(struct ta * ta, struct pending_evaluate * to_evalute, struct ssa_name name) {
    struct ssa_type * type = get_u64_hash_table(to_evalute->ssa_type_by_ssa_name, name.u16);
    if(type == NULL){
        struct ssa_type * unknown_type = CREATE_SSA_TYPE(SSA_TYPE_UNKNOWN, SSA_IR_ALLOCATOR(ta->ssa_ir));
        put_u64_hash_table(to_evalute->ssa_type_by_ssa_name, name.u16, unknown_type);
        return unknown_type;
    } else {
        return type;
    }
}

static struct u64_hash_table * get_merged_type_map_block(struct ta * ta, struct ssa_block * next_block) {
    struct u64_hash_table * merged = get_ssa_type_by_ssa_name_by_block(ta, next_block);

    FOR_EACH_U64_SET_VALUE(next_block->predecesors, predecesor_u64_ptr) {
        struct ssa_block * predecesor = (struct ssa_block *) predecesor_u64_ptr;
        struct u64_hash_table * predecesor_ssa_type_by_ssa_name = get_ssa_type_by_ssa_name_by_block(ta, predecesor);
        struct u64_hash_table_iterator predecesor_ssa_type_by_ssa_name_it;
        init_u64_hash_table_iterator(&predecesor_ssa_type_by_ssa_name_it, *predecesor_ssa_type_by_ssa_name);

        while(has_next_u64_hash_table_iterator(predecesor_ssa_type_by_ssa_name_it)) {
            struct u64_hash_table_entry current_predecesor_ssa_type_entry = next_u64_hash_table_iterator(&predecesor_ssa_type_by_ssa_name_it);
            struct ssa_type * current_predecesor_ssa_type = current_predecesor_ssa_type_entry.value;
            uint64_t current_predecesor_ssa_name_u64 = current_predecesor_ssa_type_entry.key;

            if (contains_u64_hash_table(merged, current_predecesor_ssa_name_u64)) {
                struct ssa_type * type_to_union = get_u64_hash_table(merged, current_predecesor_ssa_name_u64);
                struct ssa_type * type_result = union_type(ta, current_predecesor_ssa_type, type_to_union);

                put_u64_hash_table(merged, current_predecesor_ssa_name_u64, type_result);
            } else {
                put_u64_hash_table(merged, current_predecesor_ssa_name_u64, current_predecesor_ssa_type);
            }
        }
    }

    return merged;
}

static struct u64_hash_table * get_ssa_type_by_ssa_name_by_block(struct ta * ta, struct ssa_block * block) {
    if(!contains_u64_hash_table(&ta->ssa_type_by_ssa_name_by_block, (uint64_t) block)){
        struct u64_hash_table * ssa_type_by_ssa_name = LOX_MALLOC(&ta->ta_allocator.lox_allocator, sizeof(struct u64_hash_table));
        init_u64_hash_table(ssa_type_by_ssa_name, &ta->ta_allocator.lox_allocator);

        put_u64_hash_table(&ta->ssa_type_by_ssa_name_by_block, (uint64_t) block, ssa_type_by_ssa_name);

        return ssa_type_by_ssa_name;
    } else {
        return get_u64_hash_table(&ta->ssa_type_by_ssa_name_by_block, (uint64_t) block);
    }
}