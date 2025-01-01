#include "type_propagation.h"

struct tp {
    struct ssa_ir * ssa_ir;
    struct arena_lox_allocator tp_allocator;
    //Key block pointer: value pointer to pending_evaluate's ssa_type_by_ssa_name
    struct u64_hash_table ssa_type_by_ssa_name_by_block;

    struct u64_hash_table cyclic_ssa_name_definitions;

    struct u64_set loops_aready_scanned;
    bool rescanning_loop_body;
    int rescanning_loop_body_nested_loops;
};

struct pending_evaluate {
    struct tp * tp;
    struct u64_hash_table * ssa_type_by_ssa_name;
    struct ssa_block * block;
};

static struct tp * alloc_type_propagation(struct ssa_ir*);
static void free_type_propagation(struct tp*);
static void perform_type_propagation_block(struct pending_evaluate*);
static void perform_type_propagation_control(struct pending_evaluate *to_evalute, struct ssa_control_node *control_node);
static bool perform_type_propagation_data(struct ssa_data_node *_, void **parent_ptr, struct ssa_data_node *data_node, void *extra);
static struct ssa_type * get_type_data_node_recursive(struct pending_evaluate*, struct ssa_data_node*, struct ssa_data_node** parent_ptr);
static void add_argument_types(struct tp*, struct u64_hash_table*);
static struct ssa_type * union_type(struct tp*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_struct_types_same_definition(struct tp*, struct ssa_type*, struct ssa_type*);
static struct ssa_type * union_array_types(struct tp*, struct ssa_type*, struct ssa_type*);
static void clear_type(struct ssa_type *);
static void push_pending_evaluate(struct stack_list*, struct tp*, struct ssa_block*, struct u64_hash_table*);
static struct ssa_type * get_type_by_ssa_name(struct tp*, struct pending_evaluate*, struct ssa_name);
static struct u64_hash_table * get_merged_type_map_block(struct tp *tp, struct ssa_block *next_block);
static struct u64_hash_table * get_ssa_type_by_ssa_name_by_block(struct tp *, struct ssa_block *);
static bool is_cyclic_definition(struct tp *, struct ssa_name defined_phi, struct ssa_name);
static bool calculate_is_cyclic_definition(struct tp * tp, struct ssa_name parent, struct ssa_name);
static bool produced_type_has_been_asigned(struct ssa_data_node *);

extern void runtime_panic(char * format, ...);

void perform_type_propagation(struct ssa_ir * ssa_ir) {
    struct tp * tp = alloc_type_propagation(ssa_ir);
    struct u64_hash_table * ssa_type_by_ssa_name = LOX_MALLOC(SSA_IR_ALLOCATOR(ssa_ir), sizeof(struct u64_hash_table));
    init_u64_hash_table(ssa_type_by_ssa_name, SSA_IR_ALLOCATOR(ssa_ir));

    add_argument_types(tp, ssa_type_by_ssa_name);

    struct stack_list pending_to_evaluate;
    init_stack_list(&pending_to_evaluate, &tp->tp_allocator.lox_allocator);
    push_pending_evaluate(&pending_to_evaluate, tp, ssa_ir->first_block, ssa_type_by_ssa_name);
    put_u64_hash_table(&tp->ssa_type_by_ssa_name_by_block, (uint64_t) ssa_ir->first_block, ssa_type_by_ssa_name);

    while(!is_empty_stack_list(&pending_to_evaluate)) {
        struct pending_evaluate * to_evalute = pop_stack_list(&pending_to_evaluate);
        struct ssa_block * block_to_evalute = to_evalute->block;

        perform_type_propagation_block(to_evalute);

        switch (to_evalute->block->type_next_ssa_block) {
            case TYPE_NEXT_SSA_BLOCK_SEQ: {
                struct u64_hash_table * next_block_types = get_merged_type_map_block(to_evalute->tp,
                        block_to_evalute->next_as.next);
                push_pending_evaluate(&pending_to_evaluate, tp, block_to_evalute->next_as.next, next_block_types);
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_BRANCH: {
                struct ssa_block * false_branch = block_to_evalute->next_as.branch.false_branch;
                struct ssa_block * true_branch = block_to_evalute->next_as.branch.true_branch;

                if(block_to_evalute->is_loop_condition &&
                   tp->rescanning_loop_body &&
                   false_branch->nested_loop_body < tp->rescanning_loop_body_nested_loops) {

                    struct u64_hash_table * types_true = get_merged_type_map_block(to_evalute->tp, true_branch);
                    push_pending_evaluate(&pending_to_evaluate, tp, true_branch, types_true);
                } else {
                    struct u64_hash_table * types_false = get_merged_type_map_block(to_evalute->tp, false_branch);
                    struct u64_hash_table * types_true = get_merged_type_map_block(to_evalute->tp, true_branch);
                    push_pending_evaluate(&pending_to_evaluate, tp, false_branch, types_false);
                    push_pending_evaluate(&pending_to_evaluate, tp, true_branch, types_true);
                }
                break;
            }
            case TYPE_NEXT_SSA_BLOCK_LOOP: {
                struct ssa_block * to_jump_loop_block = block_to_evalute->next_as.loop;
                struct u64_hash_table * types_loop = clone_u64_hash_table(to_evalute->ssa_type_by_ssa_name,
                        SSA_IR_ALLOCATOR(tp->ssa_ir));

                if (!contains_u64_set(&tp->loops_aready_scanned, (uint64_t) to_jump_loop_block)) {
                    push_pending_evaluate(&pending_to_evaluate, tp, to_jump_loop_block, types_loop);
                    add_u64_set(&tp->loops_aready_scanned, (uint64_t) to_jump_loop_block);

                    tp->rescanning_loop_body_nested_loops = block_to_evalute->nested_loop_body;
                    tp->rescanning_loop_body = true;
                } else {
                    push_pending_evaluate(&pending_to_evaluate, tp, to_jump_loop_block->next_as.branch.false_branch, types_loop);
                    tp->rescanning_loop_body = false;
                }
                break;
            }
        }
    }

    ssa_ir->ssa_type_by_ssa_name_by_block = tp->ssa_type_by_ssa_name_by_block;

    free_type_propagation(tp);
}

static void perform_type_propagation_block(struct pending_evaluate * to_evalute) {
    for(struct ssa_control_node * current_control = to_evalute->block->first;; current_control = current_control->next){
        perform_type_propagation_control(to_evalute, current_control);

        if(current_control == to_evalute->block->last){
            break;
        }
    }
}

static void perform_type_propagation_control(struct pending_evaluate * to_evalute, struct ssa_control_node * control_node) {
    for_each_data_node_in_control_node(control_node, to_evalute, SSA_DATA_NODE_OPT_PRE_ORDER,
                                       perform_type_propagation_data);

    if(control_node->type == SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME){
        struct ssa_control_define_ssa_name_node * define = (struct ssa_control_define_ssa_name_node *) control_node;
        if (define->value->produced_type->type != SSA_TYPE_UNKNOWN) {
            put_u64_hash_table(to_evalute->ssa_type_by_ssa_name, define->ssa_name.u16, define->value->produced_type);
        }

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
            array_type->type = union_type(to_evalute->tp, array_type->type, set_array_element->new_element_value->produced_type);
        }
    } else if (control_node->type == SSA_CONTORL_NODE_TYPE_SET_GLOBAL) {
        struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) control_node;
        clear_type(set_global->value_node->produced_type);
    }
}

static bool perform_type_propagation_data(
        struct ssa_data_node * _,
        void ** parent_ptr,
        struct ssa_data_node * data_node,
        void * extra
) {
    struct pending_evaluate * pending_evaluate = extra;

    struct ssa_type * produced_type = get_type_data_node_recursive(pending_evaluate, data_node, (struct ssa_data_node **) parent_ptr);
    if(!(produced_type->type == SSA_TYPE_UNKNOWN && produced_type_has_been_asigned(data_node))){
        data_node->produced_type = produced_type;
    }

    return false;
}

static struct ssa_type * get_type_data_node_recursive(
        struct pending_evaluate * to_evaluate,
        struct ssa_data_node * node,
        struct ssa_data_node ** parent_ptr
) {
    struct tp * tp = to_evaluate->tp;

    switch (node->type) {
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            struct ssa_data_constant_node * constant = (struct ssa_data_constant_node *) node;
            profile_data_type_t type = lox_value_to_profile_type(constant->value);
            return CREATE_SSA_TYPE(profiled_type_to_ssa_type(type), SSA_IR_ALLOCATOR(tp->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            return CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(tp->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_LENGTH: {
            return CREATE_SSA_TYPE(SSA_TYPE_LOX_I64, SSA_IR_ALLOCATOR(tp->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            struct ssa_type * unary_operand_type = get_type_data_node_recursive(to_evaluate, unary->operand, &unary->operand);
            unary->operand->produced_type = unary_operand_type;
            return unary_operand_type;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            return get_type_by_ssa_name(tp, to_evaluate, get_ssa_name->ssa_name);
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
                        tp->ssa_ir->function, node->original_bytecode);
                struct function_call_profile function_call_profile = instruction_profile_data.as.function_call;
                returned_type = get_type_by_type_profile_data(function_call_profile.returned_type_profile);
            } else {
                returned_type = call->native_function->returned_type;
            }

            return CREATE_SSA_TYPE(profiled_type_to_ssa_type(returned_type), SSA_IR_ALLOCATOR(tp->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            struct ssa_type * right_type = get_type_data_node_recursive(to_evaluate, binary->right, &binary->right);
            struct ssa_type * left_type = get_type_data_node_recursive(to_evaluate, binary->left, &binary->left);

            binary->right->produced_type = right_type;
            binary->left->produced_type = left_type;
            ssa_type_t produced_type = binary_to_ssa_type(binary->operator, left_type->type, right_type->type,
                RETURN_LOX_TYPE_AS_DEFAULT);
            return CREATE_SSA_TYPE(produced_type, SSA_IR_ALLOCATOR(tp->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GUARD: {
            struct ssa_data_guard_node * guard = (struct ssa_data_guard_node *) node;
            switch (guard->guard.type) {
                case SSA_GUARD_ARRAY_TYPE_CHECK: {
                    struct ssa_type * array_type = CREATE_SSA_TYPE(guard->guard.value_to_compare.type, SSA_IR_ALLOCATOR(tp->ssa_ir));
                    return CREATE_ARRAY_SSA_TYPE(array_type, SSA_IR_ALLOCATOR(tp->ssa_ir));
                }
                case SSA_GUARD_TYPE_CHECK: {
                    return CREATE_SSA_TYPE(guard->guard.value_to_compare.type, SSA_IR_ALLOCATOR(tp->ssa_ir));
                }
                case SSA_GUARD_STRUCT_DEFINITION_TYPE_CHECK: {
                    struct struct_definition_object * definition = guard->guard.value_to_compare.struct_definition;
                    return CREATE_STRUCT_SSA_TYPE(definition, SSA_IR_ALLOCATOR(tp->ssa_ir));
                }
                case SSA_GUARD_BOOLEAN_CHECK: {
                    return CREATE_SSA_TYPE(SSA_TYPE_NATIVE_BOOLEAN, SSA_IR_ALLOCATOR(tp->ssa_ir));
                }
            }
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * init_struct = (struct ssa_data_initialize_struct_node *) node;
            struct ssa_type * ssa_type = CREATE_STRUCT_SSA_TYPE(init_struct->definition, SSA_IR_ALLOCATOR(tp->ssa_ir));

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
                type = CREATE_SSA_TYPE(SSA_TYPE_UNKNOWN, SSA_IR_ALLOCATOR(tp->ssa_ir));
            }

            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                struct ssa_data_node * array_element_type = init_array->elememnts_node[i];
                array_element_type->produced_type = get_type_data_node_recursive(to_evaluate, array_element_type, &init_array->elememnts_node[i]);

                if(type == NULL){
                    type = array_element_type->produced_type;
                } else if(type->type != SSA_TYPE_LOX_ANY){
                    type = !is_eq_ssa_type(type, array_element_type->produced_type) ?
                           CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(tp->ssa_ir)) :
                           array_element_type->produced_type;
                }
            }

            return CREATE_ARRAY_SSA_TYPE(type, SSA_IR_ALLOCATOR(tp->ssa_ir));
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            struct ssa_data_node * instance_node = get_struct_field->instance_node;
            struct instruction_profile_data get_struct_field_profile_data = get_instruction_profile_data_function(
                    tp->ssa_ir->function, get_struct_field->data.original_bytecode);
            struct type_profile_data get_struct_field_type_profile = get_struct_field_profile_data.as.struct_field.get_struct_field_profile;

            struct ssa_type * type_struct_instance = get_type_data_node_recursive(to_evaluate, instance_node, &get_struct_field->instance_node);

            //We insert a struct definition guard
            if (type_struct_instance->type != SSA_TYPE_LOX_STRUCT_INSTANCE) {
                struct ssa_data_guard_node * struct_instance_guard = create_from_profile_ssa_data_guard_node(get_struct_field_type_profile,
                        get_struct_field->instance_node, SSA_IR_ALLOCATOR(tp->ssa_ir));
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
                    &get_struct_field->data, SSA_IR_ALLOCATOR(tp->ssa_ir));
            struct ssa_type * type_struct_field = get_type_data_node_recursive(to_evaluate, &guard_node->data, &guard_node->guard.value);
            *parent_ptr = &guard_node->data;

            return type_struct_field;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) node;
            struct ssa_data_node * array_instance = get_array_element->instance;
            struct ssa_data_node * index = get_array_element->index;
            array_instance->produced_type = get_type_data_node_recursive(to_evaluate, array_instance, &get_array_element->instance);
            index->produced_type = get_type_data_node_recursive(to_evaluate, index, &get_array_element->index);

            //Array type found
            if(array_instance->produced_type->type == SSA_TYPE_LOX_ARRAY &&
               array_instance->produced_type->value.array->type->type != SSA_TYPE_UNKNOWN &&
               array_instance->produced_type->value.array->type->type != SSA_TYPE_LOX_ANY){
                return array_instance->produced_type->value.array->type;
            }

            struct instruction_profile_data get_array_profiled_instruction_profile_data = get_instruction_profile_data_function(
                    tp->ssa_ir->function, get_array_element->data.original_bytecode);
            struct type_profile_data get_array_profiled_data = get_array_profiled_instruction_profile_data.as.get_array_element_profile;
            profile_data_type_t get_array_profiled_type = get_type_by_type_profile_data(get_array_profiled_data);

            struct ssa_data_guard_node * array_type_guard = ALLOC_SSA_DATA_NODE(SSA_DATA_NODE_TYPE_GUARD,
                    struct ssa_data_guard_node, NULL, SSA_IR_ALLOCATOR(tp->ssa_ir));
            array_type_guard->guard.value_to_compare.type = profiled_type_to_ssa_type(get_array_profiled_type);
            array_type_guard->guard.type = SSA_GUARD_ARRAY_TYPE_CHECK;
            array_type_guard->guard.value = array_instance;
            *parent_ptr = &array_type_guard->data;

            return get_type_data_node_recursive(to_evaluate, &array_type_guard->data, parent_ptr);
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_control_define_ssa_name_node * definition_node = container_of(parent_ptr, struct ssa_control_define_ssa_name_node, value);
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) node;
            struct ssa_type * phi_type_result = NULL;
            struct u64_hash_table * types_by_ssa_name_block = get_ssa_type_by_ssa_name_by_block(tp, to_evaluate->block);

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_ssa_name) {
                struct ssa_type * type_current_ssa_name = get_type_by_ssa_name(tp, to_evaluate, current_ssa_name);

                put_u64_hash_table(types_by_ssa_name_block, current_ssa_name.u16, type_current_ssa_name);

                if(node->produced_type != NULL && node->produced_type->type != SSA_TYPE_UNKNOWN && type_current_ssa_name->type == SSA_TYPE_UNKNOWN){
                    continue;
                }
                if (phi_type_result != NULL) {
                    if(type_current_ssa_name->type == SSA_TYPE_UNKNOWN &&
                        is_cyclic_definition(tp, definition_node->ssa_name, current_ssa_name)) {
                        continue;
                    }
                    phi_type_result = union_type(tp, phi_type_result, type_current_ssa_name);
                } else {
                    phi_type_result = type_current_ssa_name;
                }
            }

            return phi_type_result;
        }

        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            runtime_panic("Illegal code path");
        }
    }
}

static struct ssa_type * union_type(
        struct tp * tp,
        struct ssa_type * a,
        struct ssa_type * b
) {
    if(a == NULL){
        return b;
    }
    if(b == NULL){
        return a;
    }
    if(a->type == SSA_TYPE_UNKNOWN || b->type == SSA_TYPE_UNKNOWN){
        return CREATE_SSA_TYPE(SSA_TYPE_UNKNOWN, SSA_IR_ALLOCATOR(tp->ssa_ir));
    } else if(a->type == b->type){
        //Different struct instances
        if(a->type == SSA_TYPE_LOX_STRUCT_INSTANCE &&
            a->value.struct_instance->definition == b->value.struct_instance->definition &&
            a->value.struct_instance != b->value.struct_instance) {
            return union_struct_types_same_definition(tp, a, b);

        } else if(a->type == SSA_TYPE_LOX_STRUCT_INSTANCE &&
                  a->value.struct_instance->definition != b->value.struct_instance->definition){
            return CREATE_STRUCT_SSA_TYPE(NULL, SSA_IR_ALLOCATOR(tp->ssa_ir));

        } else if(a->type == SSA_TYPE_LOX_ARRAY &&
            a->value.array->type->type != b->value.array->type->type) {
            return union_array_types(tp, a, b);
        } else {
            return a;
        }
    } else {
        return CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(tp->ssa_ir));
    }
}

static struct tp * alloc_type_propagation(struct ssa_ir * ssa_ir) {
    struct tp * tp =  NATIVE_LOX_MALLOC(sizeof(struct tp));
    tp->ssa_ir = ssa_ir;
    struct arena arena;
    init_arena(&arena);
    tp->tp_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&tp->ssa_type_by_ssa_name_by_block, SSA_IR_ALLOCATOR(ssa_ir));
    init_u64_set(&tp->loops_aready_scanned, &tp->tp_allocator.lox_allocator);
    init_u64_hash_table(&tp->cyclic_ssa_name_definitions, &tp->tp_allocator.lox_allocator);

    return tp;
}

static void free_type_propagation(struct tp * tp) {
    free_arena(&tp->tp_allocator.arena);
    NATIVE_LOX_FREE(tp);
}

static void add_argument_types(struct tp * tp, struct u64_hash_table * ssa_type_by_ssa_name) {
    struct function_object * function = tp->ssa_ir->function;

    for (int i = 1; i < tp->ssa_ir->function->n_arguments + 1; i++) {
        struct type_profile_data function_arg_profile_data = get_function_argument_profiled_type(function, i);
        profile_data_type_t function_arg_profiled_type = get_type_by_type_profile_data(function_arg_profile_data);

        struct ssa_name function_arg_ssa_name = CREATE_SSA_NAME(i, 0);
        struct ssa_type * function_arg_type = NULL;

        //TODO Array
        if (function_arg_profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && !function_arg_profile_data.invalid_struct_definition) {
            function_arg_type = CREATE_STRUCT_SSA_TYPE(function_arg_profile_data.struct_definition, SSA_IR_ALLOCATOR(tp->ssa_ir));
        } else {
            function_arg_type = CREATE_SSA_TYPE(profiled_type_to_ssa_type(function_arg_profiled_type), SSA_IR_ALLOCATOR(tp->ssa_ir));
        }

        put_u64_hash_table(ssa_type_by_ssa_name, function_arg_ssa_name.u16, function_arg_type);
    }
}

//Expect same definition
static struct ssa_type * union_struct_types_same_definition(
        struct tp * tp,
        struct ssa_type * a,
        struct ssa_type * b
) {
    struct struct_definition_object * definition = a->value.struct_instance->definition;
    struct ssa_type * union_struct = CREATE_STRUCT_SSA_TYPE(definition, SSA_IR_ALLOCATOR(tp->ssa_ir));
    struct struct_instance_ssa_type * struct_instance_ssa_type = union_struct->value.struct_instance;

    for(int i = 0; definition != NULL && i < definition->n_fields; i++){
        char * field_name = definition->field_names[i]->chars;
        struct ssa_type * field_a_type = get_u64_hash_table(&a->value.struct_instance->type_by_field_name, (uint64_t) field_name);
        struct ssa_type * field_b_type = get_u64_hash_table(&b->value.struct_instance->type_by_field_name, (uint64_t) field_name);

        struct ssa_type * new_field_type = field_a_type->type != field_b_type->type ?
                CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(tp->ssa_ir)) :
                union_type(tp, field_a_type, field_b_type);

        if(new_field_type->type != SSA_TYPE_UNKNOWN){
            put_u64_hash_table(&struct_instance_ssa_type->type_by_field_name, (uint64_t) field_name, new_field_type);
        }
    }

    return union_struct;
}

static struct ssa_type * union_array_types(
        struct tp * tp,
        struct ssa_type * a,
        struct ssa_type * b
) {
    struct ssa_type * new_array_type = !is_eq_ssa_type(a->value.array->type, b->value.array->type) ?
            CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, SSA_IR_ALLOCATOR(tp->ssa_ir)) :
            a->value.array->type;

    return CREATE_ARRAY_SSA_TYPE(new_array_type, SSA_IR_ALLOCATOR(tp->ssa_ir));
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
        struct tp * tp,
        struct ssa_block * block,
        struct u64_hash_table * ssa_type_by_ssa_name
) {
    struct pending_evaluate * pending_evaluate = LOX_MALLOC(&tp->tp_allocator.lox_allocator, sizeof(struct pending_evaluate));
    pending_evaluate->ssa_type_by_ssa_name = ssa_type_by_ssa_name;
    pending_evaluate->block = block;
    pending_evaluate->tp = tp;
    push_stack_list(pending_stack, pending_evaluate);
}

static struct u64_hash_table * get_merged_type_map_block(struct tp * tp, struct ssa_block * next_block) {
    struct u64_hash_table * merged = get_ssa_type_by_ssa_name_by_block(tp, next_block);

    FOR_EACH_U64_SET_VALUE(next_block->predecesors, predecesor_u64_ptr) {
        struct ssa_block * predecesor = (struct ssa_block *) predecesor_u64_ptr;
        struct u64_hash_table * predecesor_ssa_type_by_ssa_name = get_ssa_type_by_ssa_name_by_block(tp, predecesor);
        struct u64_hash_table_iterator predecesor_ssa_type_by_ssa_name_it;
        init_u64_hash_table_iterator(&predecesor_ssa_type_by_ssa_name_it, *predecesor_ssa_type_by_ssa_name);

        while(has_next_u64_hash_table_iterator(predecesor_ssa_type_by_ssa_name_it)) {
            struct u64_hash_table_entry current_predecesor_ssa_type_entry = next_u64_hash_table_iterator(&predecesor_ssa_type_by_ssa_name_it);
            struct ssa_type * current_predecesor_ssa_type = current_predecesor_ssa_type_entry.value;
            uint64_t current_predecesor_ssa_name_u64 = current_predecesor_ssa_type_entry.key;
            struct ssa_name name = CREATE_SSA_NAME_FROM_U64(current_predecesor_ssa_name_u64);

            if (contains_u64_hash_table(merged, current_predecesor_ssa_name_u64)) {
                struct ssa_type * type_to_union = get_u64_hash_table(merged, current_predecesor_ssa_name_u64);
                struct ssa_type * type_result = union_type(tp, current_predecesor_ssa_type, type_to_union);

                if(type_result->type != SSA_TYPE_UNKNOWN){
                    put_u64_hash_table(merged, current_predecesor_ssa_name_u64, type_result);
                }

            } else {
                put_u64_hash_table(merged, current_predecesor_ssa_name_u64, current_predecesor_ssa_type);
            }
        }
    }

    return merged;
}

//i2 = phi(i0, i1) i1 = i2; i2 = i1 + 1;
//is_cyclic_definition(parent = i2, name_to_check = i1) = true
static bool is_cyclic_definition(struct tp * tp, struct ssa_name parent, struct ssa_name name_to_check) {
    uint64_t cyclic_name = parent.u16 << 16 | name_to_check.u16;
    if(contains_u64_hash_table(&tp->cyclic_ssa_name_definitions, cyclic_name)){
        return (bool) get_u64_hash_table(&tp->cyclic_ssa_name_definitions, cyclic_name);
    }

    bool is_cyclic = calculate_is_cyclic_definition(tp, parent, name_to_check);
    put_u64_hash_table(&tp->cyclic_ssa_name_definitions, cyclic_name, (void *) is_cyclic);

    return is_cyclic;
}

//TODO Incorporate type info
static bool calculate_is_cyclic_definition(struct tp * tp, struct ssa_name target, struct ssa_name start) {
    struct stack_list pending_evaluate;
    init_stack_list(&pending_evaluate, &tp->tp_allocator.lox_allocator);
    push_stack_list(&pending_evaluate, (void *) start.u16);

    struct u8_set visited_ssa_names;
    init_u8_set(&visited_ssa_names);

    while(!is_empty_stack_list(&pending_evaluate)){
        struct ssa_name current_ssa_name = CREATE_SSA_NAME_FROM_U64((uint64_t) pop_stack_list(&pending_evaluate));
        struct ssa_control_define_ssa_name_node * define_ssa_name = get_u64_hash_table(
                &tp->ssa_ir->ssa_definitions_by_ssa_name, current_ssa_name.u16);
        struct u64_set used_ssa_names = get_used_ssa_names_ssa_data_node(define_ssa_name->value, &tp->tp_allocator.lox_allocator);

        FOR_EACH_U64_SET_VALUE(used_ssa_names, used_ssa_name_u16) {
            struct ssa_name used_ssa_name = CREATE_SSA_NAME_FROM_U64(used_ssa_name_u16);

            if(used_ssa_name_u16 == target.u16){
                return true;
            }
            if(contains_u8_set(&visited_ssa_names, used_ssa_name_u16)){
                continue;
            }
            if(used_ssa_name.value.local_number == target.value.local_number){
                push_stack_list(&pending_evaluate, (void *) used_ssa_name_u16);
                add_u8_set(&visited_ssa_names, used_ssa_name.value.version);
            }
        }
    }

    return false;
}

static struct u64_hash_table * get_ssa_type_by_ssa_name_by_block(struct tp * tp, struct ssa_block * block) {
    if(!contains_u64_hash_table(&tp->ssa_type_by_ssa_name_by_block, (uint64_t) block)){
        struct u64_hash_table * ssa_type_by_ssa_name = LOX_MALLOC(SSA_IR_ALLOCATOR(tp->ssa_ir), sizeof(struct u64_hash_table));
        init_u64_hash_table(ssa_type_by_ssa_name, SSA_IR_ALLOCATOR(tp->ssa_ir));

        put_u64_hash_table(&tp->ssa_type_by_ssa_name_by_block, (uint64_t) block, ssa_type_by_ssa_name);

        return ssa_type_by_ssa_name;
    } else {
        return get_u64_hash_table(&tp->ssa_type_by_ssa_name_by_block, (uint64_t) block);
    }
}

static struct ssa_type * get_type_by_ssa_name(struct tp * tp, struct pending_evaluate * to_evalute, struct ssa_name name) {
    struct ssa_type * type = get_u64_hash_table(to_evalute->ssa_type_by_ssa_name, name.u16);
    if(type == NULL){
        return CREATE_SSA_TYPE(SSA_TYPE_UNKNOWN, SSA_IR_ALLOCATOR(tp->ssa_ir));
    } else {
        return type;
    }
}

static bool produced_type_has_been_asigned(struct ssa_data_node * data_node) {
    return data_node->produced_type != NULL && data_node->produced_type->type != SSA_TYPE_UNKNOWN;
}