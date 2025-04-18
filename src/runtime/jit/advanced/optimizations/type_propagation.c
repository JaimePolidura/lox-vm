#include "type_propagation.h"

struct tp {
    struct lox_ir * lox_ir;
    struct arena_lox_allocator tp_allocator;
    //Key block pointer: value pointer to pending_evaluate's type_by_ssa_name
    struct u64_hash_table type_by_ssa_name_by_block;

    struct u64_set loops_aready_scanned;
    bool rescanning_loop_body;
    int rescanning_loop_body_nested_loops;
};

struct pending_evaluate {
    struct tp * tp;
    struct u64_hash_table * type_by_ssa_name;
    struct lox_ir_block * block;
};

static struct tp * alloc_type_propagation(struct lox_ir*);
static void free_type_propagation(struct tp*);
static void perform_type_propagation_block(struct pending_evaluate*);
static void perform_type_propagation_control(struct pending_evaluate *to_evalute, struct lox_ir_control_node *control_node);
static bool perform_type_propagation_data(struct lox_ir_data_node *_, void **parent_ptr, struct lox_ir_data_node *data_node, void *extra);
static struct lox_ir_type * get_type_data_node_recursive(struct pending_evaluate*, struct lox_ir_data_node*, struct lox_ir_data_node** parent_ptr);
static void add_argument_types(struct tp*, struct u64_hash_table*);
static struct lox_ir_type * union_type(struct tp*, struct lox_ir_type*, struct lox_ir_type*);
static struct lox_ir_type * union_struct_types_same_definition(struct tp*, struct lox_ir_type*, struct lox_ir_type*);
static struct lox_ir_type * union_array_types(struct tp*, struct lox_ir_type*, struct lox_ir_type*);
static void clear_type(struct lox_ir_type *);
static void push_pending_evaluate(struct stack_list*, struct tp*, struct lox_ir_block*, struct u64_hash_table*);
static struct lox_ir_type * get_type_by_ssa_name(struct tp*, struct pending_evaluate*, struct ssa_name);
static struct u64_hash_table * get_merged_type_map_block(struct tp *tp, struct lox_ir_block *next_block);
static struct u64_hash_table * get_type_by_ssa_name_by_block(struct tp *tp, struct lox_ir_block *block);
static bool produced_type_has_been_asigned(struct lox_ir_data_node *);
static struct lox_ir_type * merge_block_types(struct tp*, struct lox_ir_type*, struct lox_ir_type*);
static struct lox_ir_data_node * create_guard_get_struct_field(struct tp*, struct lox_ir_data_node*, struct type_profile_data);
static void set_produced_type(struct tp*, struct lox_ir_data_node*, struct lox_ir_type*);

void perform_type_propagation(struct lox_ir * lox_ir) {
    struct tp * tp = alloc_type_propagation(lox_ir);
    struct u64_hash_table * type_by_ssa_name = LOX_MALLOC(LOX_IR_ALLOCATOR(lox_ir), sizeof(struct u64_hash_table));
    init_u64_hash_table(type_by_ssa_name, LOX_IR_ALLOCATOR(lox_ir));

    add_argument_types(tp, type_by_ssa_name);

    struct stack_list pending_to_evaluate;
    init_stack_list(&pending_to_evaluate, &tp->tp_allocator.lox_allocator);
    push_pending_evaluate(&pending_to_evaluate, tp, lox_ir->first_block, type_by_ssa_name);
    put_u64_hash_table(&tp->type_by_ssa_name_by_block, (uint64_t) lox_ir->first_block, type_by_ssa_name);

    while(!is_empty_stack_list(&pending_to_evaluate)) {
        struct pending_evaluate * to_evalute = pop_stack_list(&pending_to_evaluate);
        struct lox_ir_block * block_to_evalute = to_evalute->block;

        perform_type_propagation_block(to_evalute);

        switch (to_evalute->block->type_next) {
            case TYPE_NEXT_LOX_IR_BLOCK_SEQ: {
                struct u64_hash_table * next_block_types = get_merged_type_map_block(to_evalute->tp,
                        block_to_evalute->next_as.next);
                push_pending_evaluate(&pending_to_evaluate, tp, block_to_evalute->next_as.next, next_block_types);
                break;
            }
            case TYPE_NEXT_LOX_IR_BLOCK_BRANCH: {
                struct lox_ir_block * false_branch = block_to_evalute->next_as.branch.false_branch;
                struct lox_ir_block * true_branch = block_to_evalute->next_as.branch.true_branch;

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
            case TYPE_NEXT_LOX_IR_BLOCK_LOOP: {
                struct lox_ir_block * to_jump_loop_block = block_to_evalute->next_as.loop;
                struct u64_hash_table * types_loop = clone_u64_hash_table(to_evalute->type_by_ssa_name,
                        LOX_IR_ALLOCATOR(tp->lox_ir));

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
            default:
                lox_assert_failed("type_propagation.c::perform_type_propagation", "Unknown block next type %i",
                                  to_evalute->block->type_next);
        }
    }

    lox_ir->type_by_ssa_name_by_block = tp->type_by_ssa_name_by_block;

    free_type_propagation(tp);
}

static void perform_type_propagation_block(struct pending_evaluate * to_evalute) {
    for(struct lox_ir_control_node * current_control = to_evalute->block->first;; current_control = current_control->next){
        perform_type_propagation_control(to_evalute, current_control);

        if(current_control == to_evalute->block->last){
            break;
        }
    }
}

static void perform_type_propagation_control(struct pending_evaluate * to_evalute, struct lox_ir_control_node * control_node) {
    for_each_data_node_in_lox_ir_control(control_node, to_evalute, LOX_IR_DATA_NODE_OPT_PRE_ORDER,
                                         perform_type_propagation_data);

    if (control_node->type == LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME) {
        struct lox_ir_control_define_ssa_name_node * define = (struct lox_ir_control_define_ssa_name_node *) control_node;
        put_u64_hash_table(to_evalute->type_by_ssa_name, define->ssa_name.u16, define->value->produced_type);

    } else if (control_node->type == LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD) {
        struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) control_node;
        if(set_struct_field->instance->produced_type->type == LOX_IR_TYPE_LOX_STRUCT_INSTANCE) {
            struct struct_instance_lox_ir_type * struct_instance_type = set_struct_field->instance->produced_type->value.struct_instance;
            put_u64_hash_table(&struct_instance_type->type_by_field_name, (uint64_t) set_struct_field->field_name->chars,
                               set_struct_field->new_field_value->produced_type);
        }
    } else if(control_node->type == LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT) {
        struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) control_node;
        if(set_array_element->array->produced_type->type == LOX_IR_TYPE_LOX_ARRAY) {
            struct array_lox_ir_type * array_type = set_array_element->array->produced_type->value.array;
            array_type->type = union_type(to_evalute->tp, array_type->type, set_array_element->new_element_value->produced_type);
        }
    } else if (control_node->type == LOX_IR_CONTORL_NODE_SET_GLOBAL) {
        struct lox_ir_control_set_global_node * set_global = (struct lox_ir_control_set_global_node *) control_node;
        clear_type(set_global->value_node->produced_type);
    } else {
        lox_assert_failed("type_propagation.c::perform_type_propagation_control", "Unknown control node type %i",
                          to_evalute->block->type_next);
    }
}

static bool perform_type_propagation_data(
        struct lox_ir_data_node * _,
        void ** parent_ptr,
        struct lox_ir_data_node * data_node,
        void * extra
) {
    struct pending_evaluate * pending_evaluate = extra;

    struct lox_ir_type * produced_type = get_type_data_node_recursive(pending_evaluate, data_node, (struct lox_ir_data_node **) parent_ptr);
    if(!(produced_type->type == LOX_IR_TYPE_UNKNOWN && produced_type_has_been_asigned(data_node))){
        set_produced_type(pending_evaluate->tp, data_node, produced_type);
    }

    return false;
}

static struct lox_ir_type * get_type_data_node_recursive(
        struct pending_evaluate * to_evaluate,
        struct lox_ir_data_node * node,
        struct lox_ir_data_node ** parent_ptr
) {
    struct tp * tp = to_evaluate->tp;

    switch (node->type) {
        case LOX_IR_DATA_NODE_CONSTANT: {
            struct lox_ir_data_constant_node * constant = (struct lox_ir_data_constant_node *) node;
            profile_data_type_t type = lox_value_to_profile_type(constant->value);
            return CREATE_LOX_IR_TYPE(profiled_type_to_lox_ir_type(type), LOX_IR_ALLOCATOR(tp->lox_ir));
        }
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir));
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * get_arr_length = (struct lox_ir_data_get_array_length *) node;
            set_produced_type(tp, get_arr_length->instance, get_type_data_node_recursive(to_evaluate, get_arr_length->instance, &get_arr_length->instance));
            return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_I64, LOX_IR_ALLOCATOR(tp->lox_ir));
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) node;
            struct lox_ir_type * unary_operand_type = get_type_data_node_recursive(to_evaluate, unary->operand, &unary->operand);
            set_produced_type(tp, unary->operand, unary_operand_type);

            return unary_operand_type;
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            struct lox_ir_data_get_ssa_name_node * get_ssa_name = (struct lox_ir_data_get_ssa_name_node *) node;
            return get_type_by_ssa_name(tp, to_evaluate, get_ssa_name->ssa_name);
        }
        case LOX_IR_DATA_NODE_CALL: {
            struct lox_ir_data_function_call_node * call = (struct lox_ir_data_function_call_node *) node;
            for(int i = 0; i < call->n_arguments; i++){
                struct lox_ir_data_node * argument = call->arguments[i];
                set_produced_type(tp, argument, get_type_data_node_recursive(to_evaluate, argument, &call->arguments[i]));
                //Functions might modify the array/struct. So we need to clear its type
                clear_type(argument->produced_type);
            }
            profile_data_type_t returned_type;
            if(!call->is_native) {
                struct instruction_profile_data instruction_profile_data = get_instruction_profile_data_function(
                        tp->lox_ir->function, node->original_bytecode);
                struct function_call_profile function_call_profile = instruction_profile_data.as.function_call;
                returned_type = get_type_by_type_profile_data(function_call_profile.returned_type_profile);
            } else {
                returned_type = call->native_function->returned_type;
            }

            return CREATE_LOX_IR_TYPE(profiled_type_to_lox_ir_type(returned_type), LOX_IR_ALLOCATOR(tp->lox_ir));
        }
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) node;

            struct lox_ir_type * right_type = get_type_data_node_recursive(to_evaluate, binary->right, &binary->right);
            struct lox_ir_type * left_type = get_type_data_node_recursive(to_evaluate, binary->left, &binary->left);
            set_produced_type(tp, binary->right, right_type);
            set_produced_type(tp, binary->left, left_type);

            lox_ir_type_t produced_type = binary_to_lox_ir_type(binary->operator, left_type->type, right_type->type);
            return CREATE_LOX_IR_TYPE(produced_type, LOX_IR_ALLOCATOR(tp->lox_ir));
        }
        case LOX_IR_DATA_NODE_GUARD: {
            struct lox_ir_data_guard_node * guard = (struct lox_ir_data_guard_node *) node;
            switch (guard->guard.type) {
                case LOX_IR_GUARD_ARRAY_TYPE_CHECK: {
                    struct lox_ir_type * array_type = CREATE_LOX_IR_TYPE(guard->guard.value_to_compare.type, LOX_IR_ALLOCATOR(tp->lox_ir));
                    return CREATE_ARRAY_LOX_IR_TYPE(array_type, LOX_IR_ALLOCATOR(tp->lox_ir));
                }
                case LOX_IR_GUARD_TYPE_CHECK: {
                    return CREATE_LOX_IR_TYPE(guard->guard.value_to_compare.type, LOX_IR_ALLOCATOR(tp->lox_ir));
                }
                case LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK: {
                    struct struct_definition_object * definition = guard->guard.value_to_compare.struct_definition;
                    struct lox_ir_type * type = CREATE_STRUCT_LOX_IR_TYPE(definition, LOX_IR_ALLOCATOR(tp->lox_ir));
                    if (definition == NULL) { //Expect struct, but unknown definition
                        init_u64_hash_table(&type->value.struct_instance->type_by_field_name, LOX_IR_ALLOCATOR(tp->lox_ir));
                    }

                    return type;
                }
                case LOX_IR_GUARD_BOOLEAN_CHECK: {
                    return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_BOOLEAN, LOX_IR_ALLOCATOR(tp->lox_ir));
                }
            }
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * init_struct = (struct lox_ir_data_initialize_struct_node *) node;
            struct lox_ir_type * type = CREATE_STRUCT_LOX_IR_TYPE(init_struct->definition, LOX_IR_ALLOCATOR(tp->lox_ir));

            for(int i = 0; i < init_struct->definition->n_fields; i++) {
                struct lox_ir_data_node * field_node_arg = init_struct->fields_nodes[i];
                struct lox_ir_type * field_node_type = get_type_data_node_recursive(to_evaluate, field_node_arg, &init_struct->fields_nodes[i]);
                set_produced_type(tp, field_node_arg, field_node_type);

                struct string_object * field_name = init_struct->definition->field_names[i];

                put_u64_hash_table(&type->value.struct_instance->type_by_field_name, (uint64_t) field_name->chars,
                                   field_node_type);
            }

            return type;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node * init_array = (struct lox_ir_data_initialize_array_node *) node;
            struct lox_ir_type * type = NULL;

            if (init_array->empty_initialization) {
                type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_UNKNOWN, LOX_IR_ALLOCATOR(tp->lox_ir));
            }

            for (int i = 0; i < init_array->n_elements && !init_array->empty_initialization; i++) {
                struct lox_ir_data_node * array_element_type = init_array->elememnts[i];
                set_produced_type(tp, array_element_type, get_type_data_node_recursive(to_evaluate, array_element_type, &init_array->elememnts[i]));

                if(type == NULL){
                    type = array_element_type->produced_type;
                } else if(type->type != LOX_IR_TYPE_LOX_ANY){
                    type = !is_eq_lox_ir_type(type, array_element_type->produced_type) ?
                           CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir)) :
                           array_element_type->produced_type;
                }
            }

            return CREATE_ARRAY_LOX_IR_TYPE(type, LOX_IR_ALLOCATOR(tp->lox_ir));
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * get_struct_field = (struct lox_ir_data_get_struct_field_node *) node;
            struct lox_ir_data_node * instance_node = get_struct_field->instance;
            struct instruction_profile_data get_struct_field_profile_data = get_instruction_profile_data_function(
                    tp->lox_ir->function, get_struct_field->data.original_bytecode);
            struct type_profile_data get_struct_field_type_profile = get_struct_field_profile_data.as.struct_field.get_struct_field_profile;

            struct lox_ir_type * type_struct_instance = get_type_data_node_recursive(to_evaluate, instance_node, &get_struct_field->instance);
            set_produced_type(tp, instance_node, type_struct_instance);

            if(type_struct_instance->type == LOX_IR_TYPE_UNKNOWN){
                return type_struct_instance;
            }
            //We insert a struct type-check/struct-definition-check guard
            if (type_struct_instance->type != LOX_IR_TYPE_LOX_STRUCT_INSTANCE) {
                struct lox_ir_data_node * guard_node = create_guard_get_struct_field(tp, instance_node, get_struct_field_type_profile);
                get_struct_field->instance = guard_node;
                instance_node = guard_node;

                type_struct_instance = get_type_data_node_recursive(to_evaluate, instance_node, &get_struct_field->instance);
            }

            set_produced_type(tp, instance_node, type_struct_instance);

            struct lox_ir_type * field_type = get_u64_hash_table(
                    &type_struct_instance->value.struct_instance->type_by_field_name,
                    (uint64_t) get_struct_field->field_name->chars
            );

            //Field type found
            if (field_type != NULL) {
                set_produced_type(tp, instance_node, type_struct_instance);
                return field_type;
            }
            if(get_type_by_type_profile_data(get_struct_field_type_profile) == PROFILE_DATA_TYPE_ANY){
                return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir));
            }

            //If not found, we insert a type guard in the get_struct_field
            struct lox_ir_data_guard_node * guard_node = create_from_profile_lox_ir_data_guard_node(
                    get_struct_field_type_profile,
                    &get_struct_field->data, LOX_IR_ALLOCATOR(tp->lox_ir),
                    LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER);
            struct lox_ir_type * type_struct_field = get_type_data_node_recursive(to_evaluate, &guard_node->data, &guard_node->guard.value);
            *parent_ptr = &guard_node->data;

            return type_struct_field;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_array_element = (struct lox_ir_data_get_array_element_node *) node;
            struct lox_ir_data_node * array_instance = get_array_element->instance;
            struct lox_ir_data_node * index = get_array_element->index;
            set_produced_type(tp, array_instance, get_type_data_node_recursive(to_evaluate, array_instance, &get_array_element->instance));
            set_produced_type(tp, index, get_type_data_node_recursive(to_evaluate, index, &get_array_element->index));

            //Array type found
            if(array_instance->produced_type->type == LOX_IR_TYPE_LOX_ARRAY &&
               array_instance->produced_type->value.array->type->type != LOX_IR_TYPE_UNKNOWN &&
               array_instance->produced_type->value.array->type->type != LOX_IR_TYPE_LOX_ANY){
                return array_instance->produced_type->value.array->type;
            }
            //We don't know the type yet, lets skip scanning
            if(array_instance->produced_type->type == LOX_IR_TYPE_UNKNOWN){
                return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_UNKNOWN, LOX_IR_ALLOCATOR(tp->lox_ir));
            }

            struct instruction_profile_data get_array_profiled_instruction_profile_data = get_instruction_profile_data_function(
                    tp->lox_ir->function, get_array_element->data.original_bytecode);
            struct type_profile_data get_array_profiled_data = get_array_profiled_instruction_profile_data.as.get_array_element_profile;
            profile_data_type_t get_array_profiled_type = get_type_by_type_profile_data(get_array_profiled_data);

            if (get_array_profiled_type == PROFILE_DATA_TYPE_ANY) {
                return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir));
            }

            struct lox_ir_guard guard;
            guard.value_to_compare.type = profiled_type_to_lox_ir_type(get_array_profiled_type);
            guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_RUNTIME_ERROR;
            guard.type = LOX_IR_GUARD_ARRAY_TYPE_CHECK;
            guard.value = array_instance;
            struct lox_ir_data_guard_node * array_type_guard = create_guard_lox_ir_data_node(guard, LOX_IR_ALLOCATOR(tp->lox_ir));

            get_array_element->instance = &array_type_guard->data;

            return get_type_data_node_recursive(to_evaluate, &array_type_guard->data, parent_ptr);
        }
        case LOX_IR_DATA_NODE_PHI: {
            struct lox_ir_control_define_ssa_name_node * definition_node = container_of(parent_ptr, struct lox_ir_control_define_ssa_name_node, value);
            struct lox_ir_data_phi_node * phi = (struct lox_ir_data_phi_node *) node;
            struct lox_ir_type * phi_type_result = NULL;
            struct u64_hash_table * types_by_ssa_name_block = get_type_by_ssa_name_by_block(tp, to_evaluate->block);

            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_ssa_name) {
                struct lox_ir_type * type_current_ssa_name = get_type_by_ssa_name(tp, to_evaluate, current_ssa_name);

                put_u64_hash_table(types_by_ssa_name_block, current_ssa_name.u16, type_current_ssa_name);

                if(node->produced_type != NULL && node->produced_type->type != LOX_IR_TYPE_UNKNOWN && type_current_ssa_name->type == LOX_IR_TYPE_UNKNOWN){
                    continue;
                }
                if (phi_type_result != NULL) {
                    if(type_current_ssa_name->type == LOX_IR_TYPE_UNKNOWN &&
                            is_cyclic_ssa_name_definition_lox_ir(tp->lox_ir, definition_node->ssa_name, current_ssa_name)) {
                        continue;
                    }
                    phi_type_result = union_type(tp, phi_type_result, type_current_ssa_name);
                } else {
                    phi_type_result = type_current_ssa_name;
                }
            }

            return phi_type_result;
        }
        default:
            lox_assert_failed("type_propagation.c::get_type_data_node_recursive", "Unknown data node type %i",
                              node->type);
    }
}

static struct lox_ir_data_node * create_guard_get_struct_field(
        struct tp * tp,
        struct lox_ir_data_node * instance_node,
        struct type_profile_data get_struct_type_profile
) {
    profile_data_type_t profiled_type = get_type_by_type_profile_data(get_struct_type_profile);

    struct lox_ir_guard guard;
    guard.action_on_guard_failed = LOX_IR_GUARD_FAIL_ACTION_TYPE_RUNTIME_ERROR;
    guard.type = LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK;
    guard.value_to_compare.struct_definition = NULL;
    guard.value = instance_node;
    if (profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && !get_struct_type_profile.invalid_struct_definition) {
        guard.value_to_compare.struct_definition = get_struct_type_profile.struct_definition;
    }

    struct lox_ir_data_guard_node * guard_node = create_guard_lox_ir_data_node(guard, LOX_IR_ALLOCATOR(tp->lox_ir));
    return &guard_node->data;
}

static struct lox_ir_type * union_type(
        struct tp * tp,
        struct lox_ir_type * a,
        struct lox_ir_type * b
) {
    if(a == NULL){
        return b;
    }
    if(b == NULL){
        return a;
    }
    if(a->type == LOX_IR_TYPE_UNKNOWN || b->type == LOX_IR_TYPE_UNKNOWN){
        return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_UNKNOWN, LOX_IR_ALLOCATOR(tp->lox_ir));
    } else if(a->type == b->type){
        //Different struct instances
        if(a->type == LOX_IR_TYPE_LOX_STRUCT_INSTANCE &&
            a->value.struct_instance->definition == b->value.struct_instance->definition &&
            a->value.struct_instance != b->value.struct_instance) {
            return union_struct_types_same_definition(tp, a, b);

        } else if(a->type == LOX_IR_TYPE_LOX_STRUCT_INSTANCE &&
                  a->value.struct_instance->definition != b->value.struct_instance->definition){
            return CREATE_STRUCT_LOX_IR_TYPE(NULL, LOX_IR_ALLOCATOR(tp->lox_ir));

        } else if(a->type == LOX_IR_TYPE_LOX_ARRAY &&
                is_eq_lox_ir_type(a->value.array->type, b->value.array->type)) {
            return union_array_types(tp, a, b);
        } else {
            return a;
        }
    } else {
        return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir));
    }
}

static struct tp * alloc_type_propagation(struct lox_ir * lox_ir) {
    struct tp * tp =  NATIVE_LOX_MALLOC(sizeof(struct tp));
    tp->lox_ir = lox_ir;
    struct arena arena;
    init_arena(&arena);
    tp->tp_allocator = to_lox_allocator_arena(arena);
    init_u64_hash_table(&tp->type_by_ssa_name_by_block, LOX_IR_ALLOCATOR(lox_ir));
    init_u64_set(&tp->loops_aready_scanned, &tp->tp_allocator.lox_allocator);

    return tp;
}

static void free_type_propagation(struct tp * tp) {
    free_arena(&tp->tp_allocator.arena);
    NATIVE_LOX_FREE(tp);
}

static void add_argument_types(struct tp * tp, struct u64_hash_table * type_by_ssa_name) {
    struct function_object * function = tp->lox_ir->function;

    for (int i = 1; i < tp->lox_ir->function->n_arguments + 1; i++) {
        struct type_profile_data function_arg_profile_data = get_function_argument_profiled_type(function, i);
        profile_data_type_t function_arg_profiled_type = get_type_by_type_profile_data(function_arg_profile_data);

        struct ssa_name function_arg_ssa_name = CREATE_SSA_NAME(i, 0);
        struct lox_ir_type * function_arg_type = NULL;

        if (function_arg_profiled_type == PROFILE_DATA_TYPE_STRUCT_INSTANCE && !function_arg_profile_data.invalid_struct_definition) {
            function_arg_type = CREATE_STRUCT_LOX_IR_TYPE(function_arg_profile_data.struct_definition,
                                                          LOX_IR_ALLOCATOR(tp->lox_ir));
        } else {
            function_arg_type = CREATE_LOX_IR_TYPE(profiled_type_to_lox_ir_type(function_arg_profiled_type), LOX_IR_ALLOCATOR(tp->lox_ir));
        }

        put_u64_hash_table(type_by_ssa_name, function_arg_ssa_name.u16, function_arg_type);
    }
}

//Expect same definition
static struct lox_ir_type * union_struct_types_same_definition(
        struct tp * tp,
        struct lox_ir_type * a,
        struct lox_ir_type * b
) {
    struct struct_definition_object * definition = a->value.struct_instance->definition;
    struct lox_ir_type * union_struct = CREATE_STRUCT_LOX_IR_TYPE(definition, LOX_IR_ALLOCATOR(tp->lox_ir));
    struct struct_instance_lox_ir_type * struct_instance_type = union_struct->value.struct_instance;

    for(int i = 0; definition != NULL && i < definition->n_fields; i++){
        char * field_name = definition->field_names[i]->chars;
        struct lox_ir_type * field_a_type = get_u64_hash_table(&a->value.struct_instance->type_by_field_name, (uint64_t) field_name);
        struct lox_ir_type * field_b_type = get_u64_hash_table(&b->value.struct_instance->type_by_field_name, (uint64_t) field_name);

        struct lox_ir_type * new_field_type = field_a_type->type != field_b_type->type ?
                CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir)) :
                union_type(tp, field_a_type, field_b_type);

        put_u64_hash_table(&struct_instance_type->type_by_field_name, (uint64_t) field_name, new_field_type);
    }

    return union_struct;
}

static struct lox_ir_type * union_array_types(
        struct tp * tp,
        struct lox_ir_type * a,
        struct lox_ir_type * b
) {
    struct lox_ir_type * new_array_type = !is_eq_lox_ir_type(a->value.array->type, b->value.array->type) ?
            CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, LOX_IR_ALLOCATOR(tp->lox_ir)) :
            a->value.array->type;

    return CREATE_ARRAY_LOX_IR_TYPE(new_array_type, LOX_IR_ALLOCATOR(tp->lox_ir));
}

static void clear_type(struct lox_ir_type *type) {
    if(type->type == LOX_IR_TYPE_LOX_ARRAY){
        type->value.array->type = NULL;
    } else if(type->type == LOX_IR_TYPE_LOX_STRUCT_INSTANCE){
        clear_u64_hash_table(&type->value.struct_instance->type_by_field_name);
    }
}

static void push_pending_evaluate(
        struct stack_list * pending_stack,
        struct tp * tp,
        struct lox_ir_block * block,
        struct u64_hash_table * type_by_ssa_name
) {
    struct pending_evaluate * pending_evaluate = LOX_MALLOC(&tp->tp_allocator.lox_allocator, sizeof(struct pending_evaluate));
    pending_evaluate->type_by_ssa_name = type_by_ssa_name;
    pending_evaluate->block = block;
    pending_evaluate->tp = tp;
    push_stack_list(pending_stack, pending_evaluate);
}

static struct u64_hash_table * get_merged_type_map_block(struct tp * tp, struct lox_ir_block * next_block) {
    struct u64_hash_table * merged = get_type_by_ssa_name_by_block(tp, next_block);

    FOR_EACH_U64_SET_VALUE(next_block->predecesors, struct lox_ir_block *, predecesor) {
        struct u64_hash_table * predecesor_type_by_ssa_name = get_type_by_ssa_name_by_block(tp, predecesor);
        struct u64_hash_table_iterator predecesor_type_by_ssa_name_it;
        init_u64_hash_table_iterator(&predecesor_type_by_ssa_name_it, *predecesor_type_by_ssa_name);

        while(has_next_u64_hash_table_iterator(predecesor_type_by_ssa_name_it)) {
            struct u64_hash_table_entry current_predecesor_type_entry = next_u64_hash_table_iterator(&predecesor_type_by_ssa_name_it);
            struct lox_ir_type * current_predecesor_type = current_predecesor_type_entry.value;
            uint64_t current_predecesor_ssa_name_u64 = current_predecesor_type_entry.key;
            struct ssa_name name = CREATE_SSA_NAME_FROM_U64(current_predecesor_ssa_name_u64);

            if (contains_u64_hash_table(merged, current_predecesor_ssa_name_u64)) {
                struct lox_ir_type * type_to_union = get_u64_hash_table(merged, current_predecesor_ssa_name_u64);
                struct lox_ir_type * type_result = merge_block_types(tp, current_predecesor_type, type_to_union);

                put_u64_hash_table(merged, current_predecesor_ssa_name_u64, type_result);
            } else {
                put_u64_hash_table(merged, current_predecesor_ssa_name_u64, current_predecesor_type);
            }
        }
    }

    return merged;
}

static struct lox_ir_type * merge_block_types(struct tp * tp, struct lox_ir_type * a, struct lox_ir_type * b) {
    if(a->type == LOX_IR_TYPE_UNKNOWN){
        return b;
    }
    if(b->type == LOX_IR_TYPE_UNKNOWN){
        return a;
    }

    return union_type(tp, a, b);
}

static struct u64_hash_table * get_type_by_ssa_name_by_block(struct tp * tp, struct lox_ir_block * block) {
    if(!contains_u64_hash_table(&tp->type_by_ssa_name_by_block, (uint64_t) block)){
        struct u64_hash_table * type_by_ssa_name = LOX_MALLOC(LOX_IR_ALLOCATOR(tp->lox_ir), sizeof(struct u64_hash_table));
        init_u64_hash_table(type_by_ssa_name, LOX_IR_ALLOCATOR(tp->lox_ir));

        put_u64_hash_table(&tp->type_by_ssa_name_by_block, (uint64_t) block, type_by_ssa_name);

        return type_by_ssa_name;
    } else {
        return get_u64_hash_table(&tp->type_by_ssa_name_by_block, (uint64_t) block);
    }
}

static struct lox_ir_type * get_type_by_ssa_name(struct tp * tp, struct pending_evaluate * to_evalute, struct ssa_name name) {
    struct lox_ir_type * type = get_u64_hash_table(to_evalute->type_by_ssa_name, name.u16);
    if(type == NULL){
        return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_UNKNOWN, LOX_IR_ALLOCATOR(tp->lox_ir));
    } else {
        return type;
    }
}

static bool produced_type_has_been_asigned(struct lox_ir_data_node * data_node) {
    return data_node->produced_type != NULL && data_node->produced_type->type != LOX_IR_TYPE_UNKNOWN;
}

static void set_produced_type(struct tp * tp, struct lox_ir_data_node * data, struct lox_ir_type * type) {
    data->produced_type = clone_lox_ir_type(type, LOX_IR_ALLOCATOR(tp->lox_ir));
}