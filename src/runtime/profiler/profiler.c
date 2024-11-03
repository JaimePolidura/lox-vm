#include "profiler.h"

extern __thread struct vm_thread * self_thread;

static struct instruction_profile_data * get_profile_data(struct function_object *, int instruction_index);
static void profile_unconditional_branch(struct function_object *, int instruction_index);
static void profile_conditional_branch(struct function_object *, int instruction_index);
static void profile_binary_op(struct function_object *, int instruction_index);
static void profile_binary_node(struct binary_node_profile_data *, lox_value_t lox_value);

void profile_instruction_profiler(uint8_t * pc, struct function_object * function) {
    bytecode_t instruction = (bytecode_t) *pc;
    int instruction_index = pc - function->chunk->code;

    switch (instruction) {
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
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

static void profile_binary_op(struct function_object * function, int instruction_index) {
    struct instruction_profile_data * profile_data = get_profile_data(function, instruction_index);

    profile_binary_node(&profile_data->as.binary_op.right, *(self_thread->esp - 2));
    profile_binary_node(&profile_data->as.binary_op.left, *(self_thread->esp - 1));
}

static void profile_binary_node(struct binary_node_profile_data * profile_node_data, lox_value_t lox_value) {
    if(IS_STRING(lox_value)){
        atomic_fetch_add(&profile_node_data->string, 1);
    } else if (IS_NUMBER(lox_value)) {
        double double_value = AS_NUMBER(lox_value);
        if (has_decimals(double_value)) {
            atomic_fetch_add(&profile_node_data->f64, 1);
        } else {
            atomic_fetch_add(&profile_node_data->i64, 1);
        }
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