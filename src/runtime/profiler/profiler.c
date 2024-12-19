#include "profiler.h"

extern __thread struct vm_thread * self_thread;

static struct instruction_profile_data * get_profile_data(struct function_object *, int instruction_index);
static void profile_conditional_branch(struct function_object *, int instruction_index);
static void profile_type(struct type_profile_data *profile_node_data, lox_value_t lox_value);
static void profile_struct_field(struct function_object *, int instruction_index);
static void profile_get_struct_field(struct function_object *, int instruction_index);
static void profile_function_call(struct function_object *, int instruction_index);
static void profile_get_array_elemet(struct function_object *, int instruction_index);

void profile_instruction_profiler(uint8_t * pc, struct function_object * function) {
    bytecode_t instruction = (bytecode_t) *pc;
    int instruction_index = pc - function->chunk->code;

    switch (instruction) {
        case OP_GET_ARRAY_ELEMENT:
            profile_get_array_elemet(function, instruction_index);
            break;
        case OP_GET_STRUCT_FIELD:
            profile_get_struct_field(function, instruction_index);
            profile_struct_field(function, instruction_index);
            break;
        case OP_SET_STRUCT_FIELD:
            profile_struct_field(function, instruction_index);
            break;
        case OP_JUMP_IF_FALSE:
            profile_conditional_branch(function, instruction_index);
            break;
        default:
            break;
    }
}

static void profile_get_struct_field(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * instruction_profile = get_profile_data(function, instruction_index);
    lox_value_t field_name_lox_value = *(self_thread->esp - 2);
    lox_value_t struct_lox_value = *(self_thread->esp - 1);
    if(!IS_STRING(field_name_lox_value)){
        return;
    }
    if(!IS_OBJECT(struct_lox_value) || AS_OBJECT(struct_lox_value)->type != OBJ_STRUCT_INSTANCE){
        return;
    }
    struct struct_instance_object * instance = (struct struct_instance_object *) AS_OBJECT(struct_lox_value);
    struct string_object * field_name = AS_STRING_OBJECT(field_name_lox_value);
    if(!contains_hash_table(&instance->fields, field_name)){
        return;
    }

    lox_value_t field_value;
    get_hash_table(&instance->fields, field_name, &field_value);

    profile_type(&instruction_profile->as.struct_field.get_struct_field_profile, field_value);
}

static void profile_get_array_elemet(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * instruction_profile = get_profile_data(function, instruction_index);
    lox_value_t array_lox_value = *(self_thread->esp - 1);
    lox_value_t index_lox_value = *(self_thread->esp - 2);
    if(!IS_NUMBER(index_lox_value)){
        return;
    }
    if(!IS_OBJECT(array_lox_value) || AS_OBJECT(array_lox_value)->type != OBJ_ARRAY){
        return;
    }
    struct array_object * array = (struct array_object *) AS_OBJECT(array_lox_value);
    uint64_t index = AS_NUMBER(index_lox_value);
    if(array->values.capacity <= index){
        return;
    }

    profile_type(&instruction_profile->as.get_array_element_profile, array->values.values[index]);
}

void profile_function_call_argumnts(struct function_object * callee) {
    struct function_profile_data * callee_profile_data = &callee->state_as.profiling.profile_data;
    struct call_frame * callee_frame = get_current_frame_vm_thread(self_thread);

    if (callee_profile_data->arguments_type_profile == NULL && callee->n_arguments > 0) {
        struct type_profile_data * arguments_type_profile = NATIVE_LOX_MALLOC(sizeof(struct type_profile_data) * callee->n_arguments);
        callee_profile_data->arguments_type_profile = arguments_type_profile;
        callee_profile_data->n_arguments = callee->n_arguments;
    }

    for (int i = 0; i < callee->n_arguments; i++) {
        lox_value_t current_argument = callee_frame->slots[i + 1];
        profile_type(&callee_profile_data->arguments_type_profile[i], current_argument);
    }
}

void profile_returned_value(uint8_t * pc, struct function_object * caller, lox_value_t returned_value) {
    int instruction_index = pc - caller->chunk->code;
    struct instruction_profile_data * profile_data = get_profile_data(caller, instruction_index);
    struct function_call_profile * function_call_profile = &profile_data->as.function_call;
    profile_type(&function_call_profile->returned_type_profile, returned_value);
}

bool can_jit_compile_profiler(struct function_object * function) {
    return function->state_as.not_profiling.n_calls >= MIN_CALLS_TO_JIT_COMPILE;
}

static void profile_struct_field(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * profile_data = get_profile_data(function, instruction_index);
    lox_value_t struct_instance_lox_value = *(self_thread->esp - 1);

    if(IS_OBJECT(struct_instance_lox_value) && AS_OBJECT(struct_instance_lox_value)->type != OBJ_STRUCT_INSTANCE){
        struct struct_instance_object * struct_instance = (struct struct_instance_object *) AS_OBJECT(struct_instance_lox_value);
        if(!profile_data->as.struct_field.initialized){
            profile_data->as.struct_field.definition = struct_instance->definition;
            profile_data->as.struct_field.initialized = true;
        } else if (profile_data->as.struct_field.initialized && profile_data->as.struct_field.definition != struct_instance->definition) {
            profile_data->as.struct_field.definition = NULL; //Mark that the struct struct_definition could be any type
        }
    }
}

static void profile_type(struct type_profile_data * profile_node_data, lox_value_t lox_value) {
    switch (lox_value_to_profile_type(lox_value)) {
        case PROFILE_DATA_TYPE_I64:
            atomic_fetch_add(&profile_node_data->i64, 1);
            break;
        case PROFILE_DATA_TYPE_F64:
            atomic_fetch_add(&profile_node_data->f64, 1);
            break;
        case PROFILE_DATA_TYPE_STRING:
            atomic_fetch_add(&profile_node_data->string, 1);
            break;
        case PROFILE_DATA_TYPE_BOOLEAN:
            atomic_fetch_add(&profile_node_data->boolean, 1);
            break;
        case PROFILE_DATA_TYPE_NIL:
            atomic_fetch_add(&profile_node_data->nil, 1);
            break;
        case PROFILE_DATA_TYPE_STRUCT_INSTANCE:
            atomic_fetch_add(&profile_node_data->struct_instance, 1);
            break;
        case PROFILE_DATA_TYPE_ARRAY:
            atomic_fetch_add(&profile_node_data->array, 1);
            break;
        case PROFILE_DATA_TYPE_ANY:
            atomic_fetch_add(&profile_node_data->any, 1);
            break;
    }
}

static void profile_conditional_branch(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * profile_data = get_profile_data(function, instruction_index);
    lox_value_type bool_value = *(self_thread->esp - 1);

    if (AS_BOOL(bool_value)) {
        atomic_fetch_add(&profile_data->as.branch.taken, 1);
    } else {
        atomic_fetch_add(&profile_data->as.branch.not_taken, 1);
    }
}

static struct instruction_profile_data * get_profile_data(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * per_instruction = function->state_as.profiling.profile_data.profile_by_instruction_index;
    struct instruction_profile_data * instruction_profile_data = &per_instruction[instruction_index];
    return instruction_profile_data;
}