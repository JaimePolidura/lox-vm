#include "x86_jit_compiler.h"

extern bool get_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t *value);
extern void print_lox_value(lox_value_t value);
extern void runtime_panic(char * format, ...);

extern __thread struct vm_thread * self_thread;

struct cpu_regs_state save_cpu_state() {
    return (struct cpu_regs_state) {
        //TODO
    };
}

void restore_cpu_state(struct cpu_regs_state * regs) {
    //TODO
}

#define READ_BYTECODE(jit_compiler) (*(jit_compiler)->pc++)
#define READ_U16(jit_compiler) \
    ((jit_compiler)->pc += 2, (uint16_t)(((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_CONSTANT(jit_compiler) (jit_compiler->function_to_compile->chunk.constants.values[READ_BYTECODE(jit_compiler)])
#define CURRENT_BYTECODE_INDEX(jit_compiler) (jit_compiler->pc - jit_compiler->function_to_compile->chunk.code)

static struct jit_compiler init_jit_compiler(struct function_object * function);
static void add(struct jit_compiler * jit_compiler);
static void sub(struct jit_compiler * jit_compiler);
static void lox_value(struct jit_compiler * jit_compiler, lox_value_t value, int bytecode_instruction_length);
static void comparation(struct jit_compiler * jit_compiler, op_code comparation_opcode, int bytecode_instruction_length);
static void greater(struct jit_compiler * jit_compiler);
static void negate(struct jit_compiler * jit_compiler);
static void multiplication(struct jit_compiler * jit_compiler);
static void division(struct jit_compiler * jit_compiler);
static void loop(struct jit_compiler * jit_compiler, uint16_t bytecode_backward_jump);
static void constant(struct jit_compiler * jit_compiler);
static void jump(struct jit_compiler * jit_compiler, uint16_t offset);
static void jump_if_false(struct jit_compiler * jit_compiler, uint16_t jump_offset);
static void print(struct jit_compiler * jit_compiler);
static void pop(struct jit_compiler * jit_compiler);
static void get_local(struct jit_compiler * jit_compiler);
static void set_local(struct jit_compiler * jit_compiler);
static void not(struct jit_compiler * jit_compiler);
static void enter_package(struct jit_compiler * jit_compiler);
static void exit_package(struct jit_compiler * jit_compiler);

static void get_global(struct jit_compiler * jit_compiler);

static void record_pending_jump_to_patch(struct jit_compiler * jit_compiler, uint16_t jump_instruction_index, uint16_t bytecode_offset);
static void record_compiled_bytecode(struct jit_compiler * jit_compiler, uint16_t native_compiled_index, int bytecode_instruction_length);
static void set_al_with_cmp_result(struct jit_compiler * jit_compiler, op_code comparation_opcode);
static void number_const(struct jit_compiler * jit_compiler, int value, int instruction_length);
static void cast_to_lox_boolean(struct jit_compiler * jit_compiler, register_t register_boolean_value);
static void check_pending_jumps_to_patch(struct jit_compiler * jit_compiler, int bytecode_instruction_length);
static void free_jit_compiler(struct jit_compiler * jit_compiler);

struct jit_compilation_result jit_compile(struct function_object * function) {
    struct jit_compiler jit_compiler = init_jit_compiler(function);
    bool finish_compilation_flag = false;

    prepare_x64_stack(&jit_compiler.native_compiled_code);
    push_stack(&jit_compiler.package_stack, self_thread->current_package);

    for(;;) {
        switch (READ_BYTECODE(&jit_compiler)) {
            case OP_CONST_1: number_const(&jit_compiler, 1, OP_CONST_1_LENGTH); break;
            case OP_CONST_2: number_const(&jit_compiler, 2, OP_CONST_2_LENGTH); break;
            case OP_CONSTANT: constant(&jit_compiler); break;
            case OP_FAST_CONST_8: number_const(&jit_compiler, READ_BYTECODE(&jit_compiler), OP_FAST_CONST_8_LENGTH); break;
            case OP_FAST_CONST_16: number_const(&jit_compiler, READ_U16(&jit_compiler), OP_FAST_CONST_16_LENGTH); break;
            case OP_PRINT: print(&jit_compiler); break;
            case OP_ADD: add(&jit_compiler); break;
            case OP_SUB: sub(&jit_compiler); break;
            case OP_NEGATE: negate(&jit_compiler); break;
            case OP_POP: pop(&jit_compiler); break;
            case OP_TRUE: lox_value(&jit_compiler, TRUE_VALUE, OP_TRUE_LENGTH); break;
            case OP_FALSE: lox_value(&jit_compiler, FALSE_VALUE, OP_FALSE_LENGTH); break;
            case OP_NIL: lox_value(&jit_compiler, NIL_VALUE, OP_NIL_LENGTH); break;
            case OP_EQUAL: comparation(&jit_compiler, OP_EQUAL, OP_EQUAL_LENGTH); break;
            case OP_GREATER: comparation(&jit_compiler, OP_GREATER, OP_GREATER_LENGTH); break;
            case OP_LESS: comparation(&jit_compiler, OP_LESS, OP_LESS_LENGTH); break;
            case OP_GET_LOCAL: get_local(&jit_compiler); break;
            case OP_SET_LOCAL: set_local(&jit_compiler); break;
            case OP_MUL: multiplication(&jit_compiler); break;
            case OP_DIV: division(&jit_compiler); break;
            case OP_JUMP_IF_FALSE: jump_if_false(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_JUMP: jump(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_LOOP: loop(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_NOT: not(&jit_compiler); break;
            case OP_EOF: finish_compilation_flag = true; break;
            case OP_GET_GLOBAL: get_global(&jit_compiler); break;
            case OP_ENTER_PACKAGE: enter_package(&jit_compiler); break;
            case OP_ENTER_PACKAGE: exit_package(&jit_compiler); break;
            case OP_NO_OP: break;
            default: runtime_panic("Unhandled bytecode to compile %u\n", *(--jit_compiler.pc));
        }

        if(finish_compilation_flag){
            break;
        }
    }
    
    end_x64_stack(&jit_compiler.native_compiled_code);

    free_jit_compiler(&jit_compiler);

    return (struct jit_compilation_result) {
            .compiled_code = jit_compiler.native_compiled_code,
            .success = true,
    };
}

static void enter_package(struct jit_compiler * jit_compiler) {

}

static void exit_package(struct jit_compiler * jit_compiler) {

}

static void get_global(struct jit_compiler * jit_compiler) {
    struct package * current_package = peek_stack_list(jit_compiler->package_stack);

    register_t variable_name_register = peek_register_allocator(&jit_compiler->register_allocator);    
    register_t package_addr_register = push_register_allocator(&jit_compiler->register_allocator);    
    register_t package_variable_value = push_register_allocator(&jit_compiler->register_allocator);    
    
    emit_mov(&jit_compiler->native_compiled_code, 
        REGISTER_TO_OPERAND(package_addr_register), 
        current_package);

    emit_sub(&jit_compiler->native_compiled_code, 
        RSP_OPERAND, 
        IMMEDIATE_TO_OPERAND(sizeof(void *)));

    call_external_c_function(jit_compiler->native_compiled_code, &get_hash_table, 3, 
        current_package->global_variables, 
        variable_name_register, );

    emit_add(&jit_compiler->native_compiled_code, 
        RSP_OPERAND, 
        IMMEDIATE_TO_OPERAND(sizeof(void *)));
}

//True value:  0x7ffc000000000003
//False value: 0x7ffc000000000002
//value1 = value0 + ((TRUE - value0) + (FALSE - value0))
//value1 = - value0 + TRUE + FALSE
//value1 = (TRUE - value) + FALSE
//value1 = value0 + d1 + d2
static void not(struct jit_compiler * jit_compiler) {
    register_t value_to_negate = pop_register_allocator(&jit_compiler->register_allocator);

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(value_to_negate),
             IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(value_to_negate),
             IMMEDIATE_TO_OPERAND(FALSE_VALUE));

    push_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, jit_compiler->native_compiled_code.in_use - 2, OP_NOT_LENGTH);
}

static void set_local(struct jit_compiler * jit_compiler) {
    uint8_t slot = READ_BYTECODE(jit_compiler);

    if(slot > jit_compiler->last_local_count_slot){
        uint8_t diff = slot - jit_compiler->last_local_count_slot;
        uint8_t stack_grow = diff * sizeof(lox_value_t);
        emit_sub(&jit_compiler->native_compiled_code, IMMEDIATE_TO_OPERAND(stack_grow), RSP_OPERAND);
    }

    register_t register_local_value = pop_register_allocator(&jit_compiler->register_allocator);
    int offset_local_from_rbp = slot * sizeof(lox_value_t);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(RBP, offset_local_from_rbp),
             REGISTER_TO_OPERAND(register_local_value));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_SET_LOCAL_LENGTH);
}

static void get_local(struct jit_compiler * jit_compiler) {
    uint8_t slot = READ_BYTECODE(jit_compiler);

    register_t register_to_save_local = push_register_allocator(&jit_compiler->register_allocator);
    int offset_local_from_rbp = slot * sizeof(lox_value_t);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_to_save_local),
             DISPLACEMENT_TO_OPERAND(RBP, - offset_local_from_rbp)); //Stack grows to lower address

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_LOCAL_LENGTH);
}

static void print(struct jit_compiler * jit_compiler) {
    register_t to_print_register_arg = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            &jit_compiler->native_compiled_code,
            (uint64_t *) &print_lox_value,
            1,
            REGISTER_TO_OPERAND(to_print_register_arg));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_PRINT_LENGTH);
}

static void jump_if_false(struct jit_compiler * jit_compiler, uint16_t jump_offset) {
    register_t lox_boolean_register = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t cmp_index = emit_cmp(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(lox_boolean_register),
                                  IMMEDIATE_TO_OPERAND(TRUE_VALUE));
    uint16_t jmp_index = emit_near_je(&jit_compiler->native_compiled_code, 0);

    record_pending_jump_to_patch(jit_compiler, jmp_index, jump_offset);
    record_compiled_bytecode(jit_compiler, cmp_index, OP_JUMP_IF_FALSE_LENGTH);
}

static void jump(struct jit_compiler * jit_compiler, uint16_t offset) {
    uint16_t jmp_index = emit_near_jmp(&jit_compiler->native_compiled_code, 0); //We don't know the offset of where to jump

    record_pending_jump_to_patch(jit_compiler, jmp_index, offset);
    record_compiled_bytecode(jit_compiler, jmp_index, OP_JUMP_LENGTH);
}

static void pop(struct jit_compiler * jit_compiler) {
    pop_register_allocator(&jit_compiler->register_allocator);
    record_compiled_bytecode(jit_compiler, -1, OP_POP_LENGTH);
}

static void loop(struct jit_compiler * jit_compiler, uint16_t bytecode_backward_jump) {
    uint16_t bytecode_index_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_backward_jump - OP_LOOP_LENGTH;
    uint16_t native_index_to_jump = jit_compiler->compiled_bytecode_to_native_by_index[bytecode_index_to_jump];
    uint16_t current_native_index = jit_compiler->native_compiled_code.in_use;

    // +5 because of instruction size
    uint16_t jmp_offset = (current_native_index - native_index_to_jump) + 5;

    //Loop bytecode instruction always jumps backward
    uint16_t jmp_index = emit_near_jmp(&jit_compiler->native_compiled_code, -((int) jmp_offset));

    record_compiled_bytecode(jit_compiler, jmp_index, OP_LOOP_LENGTH);
}

static void constant(struct jit_compiler * jit_compiler) {
    lox_value_t constant = READ_CONSTANT(jit_compiler);

    register_t register_to_store_constant = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(register_to_store_constant),
                                  IMMEDIATE_TO_OPERAND(constant));

    record_compiled_bytecode(jit_compiler, mov_index, OP_CONSTANT_LENGTH);
}

static void division(struct jit_compiler * jit_compiler) {
    register_t b = pop_register_allocator(&jit_compiler->register_allocator);
    register_t a = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  RAX_OPERAND,
                                  IMMEDIATE_TO_OPERAND(b));

    emit_idiv(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(a));

    register_t result_register = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(result_register),
             RAX_OPERAND);

    record_compiled_bytecode(jit_compiler, mov_index, OP_DIV_LENGTH);
}

static void multiplication(struct jit_compiler * jit_compiler) {
    register_t b = pop_register_allocator(&jit_compiler->register_allocator);
    register_t a = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t native_offset = emit_imul(&jit_compiler->native_compiled_code,
                                       REGISTER_TO_OPERAND(a),
                                       REGISTER_TO_OPERAND(b));

    push_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, native_offset, OP_MUL_LENGTH);
}

static void negate(struct jit_compiler * jit_compiler) {
    register_t register_to_negate = pop_register_allocator(&jit_compiler->register_allocator);
    uint16_t native_index = emit_neg(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(register_to_negate));
    push_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, native_index, OP_NEGATE_LENGTH);
}

static void comparation(struct jit_compiler * jit_compiler, op_code comparation_opcode, int bytecode_instruction_length) {
    register_t b = pop_register_allocator(&jit_compiler->register_allocator);
    register_t a = pop_register_allocator(&jit_compiler->register_allocator);
    uint16_t native_offset = emit_cmp(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(a), REGISTER_TO_OPERAND(b));

    uint8_t next_bytecode = *(jit_compiler->pc + 1);
    if(next_bytecode != OP_JUMP_IF_FALSE && next_bytecode != OP_LOOP){
        set_al_with_cmp_result(jit_compiler, comparation_opcode);

        register_t register_casted_value = push_register_allocator(&jit_compiler->register_allocator);

        cast_to_lox_boolean(jit_compiler, register_casted_value);
    }

    record_compiled_bytecode(jit_compiler, native_offset, bytecode_instruction_length);
}

static void set_al_with_cmp_result(struct jit_compiler * jit_compiler, op_code comparation_opcode) {
    switch (comparation_opcode) {
        case OP_EQUAL: emit_sete_al(&jit_compiler->native_compiled_code); break;
        case OP_GREATER: emit_setg_al(&jit_compiler->native_compiled_code); break;
        case OP_LESS: emit_setl_al(&jit_compiler->native_compiled_code); break;
        default: //TODO Panic
    }
}

static void cast_to_lox_boolean(struct jit_compiler * jit_compiler, register_t register_casted_value) {
    //If true register_casted_value will hold 1, if false it will hold 0
    emit_al_movzx(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(register_casted_value));

    //If true, register_casted_value will hold 1, if you add 2, you will get the value of TAG_TRUE defined in types.h
    //If false, register_casted_value will hold 0, if you add 2, you will get the value of TAG_FALSE defined in types.h
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_casted_value),
             IMMEDIATE_TO_OPERAND(2));

    emit_or(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(register_casted_value),
            IMMEDIATE_TO_OPERAND(QUIET_FLOAT_NAN));
}

static void lox_value(struct jit_compiler * jit_compiler, lox_value_t value, int bytecode_instruction_length) {
    register_t reg = push_register_allocator(&jit_compiler->register_allocator);
    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(reg),
                                  IMMEDIATE_TO_OPERAND(value));

    record_compiled_bytecode(jit_compiler, mov_index, bytecode_instruction_length);
}

static void number_const(struct jit_compiler * jit_compiler, int value, int instruction_length) {
    register_t reg = push_register_allocator(&jit_compiler->register_allocator);
    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(reg),
                                  IMMEDIATE_TO_OPERAND(value));

    record_compiled_bytecode(jit_compiler, mov_index, instruction_length);
}

static void sub(struct jit_compiler * jit_compiler) {
    register_t operandB = pop_register_allocator(&jit_compiler->register_allocator);
    register_t operandA = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t sub_index = emit_sub(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(operandA),
                                  REGISTER_TO_OPERAND(operandB));
    push_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, sub_index, OP_SUB_LENGTH);
}

static void add(struct jit_compiler * jit_compiler) {
    register_t operandB = pop_register_allocator(&jit_compiler->register_allocator);
    register_t operandA = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t add_index = emit_add(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(operandA),
                                  REGISTER_TO_OPERAND(operandB));

    push_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, add_index, OP_ADD_LENGTH);
}

static void record_pending_jump_to_patch(struct jit_compiler * jit_compiler, uint16_t jump_instruction_index, uint16_t bytecode_offset) {
    uint16_t bytecode_instruction_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) + bytecode_offset;

    if(jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump] == NULL) {
        jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump] = malloc(sizeof(struct pending_path_jump));
    }

    //Near jump instruction: (opcode 1 byte) + (offset 4 bytes) TODO check return value
    add_pending_path_jump(jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump], jump_instruction_index + 1);
}

static struct jit_compiler init_jit_compiler(struct function_object * function) {
    struct jit_compiler compiler;

    compiler.compiled_bytecode_to_native_by_index = malloc(sizeof(uint16_t) * function->chunk.in_use);
    memset(compiler.compiled_bytecode_to_native_by_index, 0, sizeof(uint16_t) * function->chunk.in_use);

    compiler.pending_jumps_to_patch = malloc(sizeof(void *) * function->chunk.in_use);
    memset(compiler.pending_jumps_to_patch, 0, sizeof(void *) * function->chunk.in_use);

    compiler.last_local_count_slot = function->n_arguments;
    compiler.function_to_compile = function;
    compiler.pc = function->chunk.code;
    
    init_register_allocator(&compiler.register_allocator);
    init_u8_arraylist(&compiler.native_compiled_code);
    init_stack_list(&compiler.package_stack);

    return compiler;
}

static void record_compiled_bytecode(struct jit_compiler * jit_compiler, uint16_t native_compiled_index, int bytecode_instruction_length) {
    uint16_t bytecode_compiled_index = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_instruction_length;

    jit_compiler->compiled_bytecode_to_native_by_index[bytecode_compiled_index] = native_compiled_index;

    check_pending_jumps_to_patch(jit_compiler, bytecode_instruction_length);
}

static void check_pending_jumps_to_patch(struct jit_compiler * jit_compiler, int bytecode_instruction_length) {
    uint16_t current_bytecode_index = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_instruction_length;
    uint16_t current_compiled_index = jit_compiler->compiled_bytecode_to_native_by_index[current_bytecode_index];

    struct pending_path_jump * pending_jumps = jit_compiler->pending_jumps_to_patch[current_bytecode_index];

    if(pending_jumps != NULL) {
        for(int i = 0; i < MAX_JUMPS_REFERENCES_TO_LINE; i++){
            uint16_t compiled_native_jmp_offset_index = pending_jumps->compiled_native_jmp_offset_index[i];

            if (compiled_native_jmp_offset_index != 0) {
                //compiled_native_jmp_offset_index points to native jmp offset part of the instruction
                uint16_t native_jmp_offset = current_compiled_index - (compiled_native_jmp_offset_index - 1);
                uint16_t * compiled_native_jmp_offset_index_ptr = (uint16_t *) (jit_compiler->native_compiled_code.values + compiled_native_jmp_offset_index);

                *compiled_native_jmp_offset_index_ptr = native_jmp_offset;
            }
        }

        jit_compiler->pending_jumps_to_patch[current_bytecode_index] = NULL;
        free(pending_jumps);
    }
}

static void free_jit_compiler(struct jit_compiler * jit_compiler) {
    for(int i = 0; i < jit_compiler->function_to_compile->chunk.in_use; i++){
        if(jit_compiler->pending_jumps_to_patch[i] != NULL){
            free(jit_compiler->pending_jumps_to_patch[i]);
        }
    }
    clear_stack(&jit_compiler->package_stack);
    free(jit_compiler->pending_jumps_to_patch);
    free(jit_compiler->compiled_bytecode_to_native_by_index);
}