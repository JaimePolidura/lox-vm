#include "profiler.h"

extern __thread struct vm_thread * self_thread;

static struct instruction_profile_data * get_profile_data(struct function_object *, int instruction_index);
static void profile_unconditional_branch(struct function_object *, int instruction_index);
static void profile_conditional_branch(struct function_object *, int instruction_index);
static void profile_binary_op(struct function_object *, int instruction_index);
static void profile_type(struct type_profile_data *profile_node_data, lox_value_t lox_value);
static void profile_struct_field(struct function_object *, int instruction_index);
static void profile_set_local_variable(struct function_object * function, uint8_t * pc);
static void profile_get_local_variable(struct function_object * function, uint8_t * pc);

void profile_instruction_profiler(uint8_t * pc, struct function_object * function) {
    bytecode_t instruction = (bytecode_t) *pc;
    int instruction_index = pc - function->chunk->code;

    switch (instruction) {
        case OP_GET_STRUCT_FIELD:
        case OP_SET_STRUCT_FIELD:
            profile_struct_field(function, instruction_index);
            break;
        case OP_GET_LOCAL:
            profile_get_local_variable(function, pc);
            break;
        case OP_SET_LOCAL:
            profile_set_local_variable(function, pc);
            break;
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
        case OP_BINARY_OP_AND:
        case OP_BINARY_OP_OR:
        case OP_RIGHT_SHIFT:
        case OP_LEFT_SHIFT:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
            profile_binary_op(function, instruction_index);
            break;
        case OP_JUMP_IF_FALSE:
            profile_conditional_branch(function, instruction_index);
            break;
        case OP_JUMP:
        case OP_LOOP:
        case OP_CALL:
            profile_unconditional_branch(function, instruction_index);
            break;
        default:
            break;
    }
}

bool can_jit_compile_profiler(struct function_object * function) {
    return function->state_as.not_profiling.n_calls >= MIN_CALLS_TO_JIT_COMPILE;
}

static void profile_get_local_variable(struct function_object * function, uint8_t * pc) {
    uint8_t slot = *(pc + 1);
    lox_value_t local_variable_value = self_thread->frames[self_thread->frames_in_use - 1].slots[slot];
    struct function_profile_data * profile_data = &function->state_as.profiling.profile_data;
    struct type_profile_data * local_variable_profile = &profile_data->local_profile_data[slot];
    profile_type(local_variable_profile, local_variable_value);
}

static void profile_set_local_variable(struct function_object * function, uint8_t * pc) {
    uint8_t slot = *(pc + 1);
    lox_value_t local_variable_value = *(self_thread->esp - 1);
    struct function_profile_data * profile_data = &function->state_as.profiling.profile_data;
    struct type_profile_data * local_variable_profile = &profile_data->local_profile_data[slot];
    profile_type(local_variable_profile, local_variable_value);
}

static void profile_struct_field(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * profile_data = get_profile_data(function, instruction_index);

    struct call_frame * current_call_frame = &self_thread->frames[self_thread->frames_in_use - 1];
    lox_value_type value = current_call_frame->function->chunk->constants.values[*current_call_frame->pc];

    profile_type(&profile_data->as.struct_field.type, value);
}

static void profile_binary_op(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * profile_data = get_profile_data(function, instruction_index);

    profile_type(&profile_data->as.binary_op.left, *(self_thread->esp - 2));
    profile_type(&profile_data->as.binary_op.left, *(self_thread->esp - 1));
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
        case PROFILE_DATA_TYPE_OBJECT:
            atomic_fetch_add(&profile_node_data->object, 1);
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

static void profile_unconditional_branch(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * profile_data = get_profile_data(function, instruction_index);
    atomic_fetch_add(&profile_data->as.branch.taken, 1);
}

static struct instruction_profile_data * get_profile_data(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * per_instruction = function->state_as.profiling.profile_data.data_by_instruction_index;
    struct instruction_profile_data * instruction_profile_data = &per_instruction[instruction_index];
    return instruction_profile_data;
}