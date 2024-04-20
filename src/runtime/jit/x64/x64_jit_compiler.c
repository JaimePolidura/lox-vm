#include "x64_jit_compiler.h"

#define INCREASE_MONITOR_COUNT true
#define DECREASE_MONITOR_COUNT true

extern bool get_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t *value);
extern bool put_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);
extern struct call_frame * get_current_frame_vm_thread(struct vm_thread *);
extern bool set_element_array(struct array_object * array, int index, lox_value_t value);
extern struct struct_instance_object * alloc_struct_instance_object();
extern struct array_object * alloc_array_object(int size);
extern void add_object_to_heap(struct object * object);
extern void enter_monitor(struct monitor * monitor);
extern void exit_monitor(struct monitor * monitor);
extern void print_lox_value(lox_value_t value);
extern void runtime_panic(char * format, ...);
extern void check_gc_on_safe_point_alg();
extern void set_self_thread_runnable();
extern void set_self_thread_waiting();
extern bool restore_prev_call_frame();
extern void switch_vm_to_jit_mode(struct jit_compiler *);
extern void setup_vm_to_jit_mode(struct jit_compiler *);

//Used by jit_compiler::compiled_bytecode_to_native_by_index Some instructions are not compiled to native code, but some jumps bytecode offset
//will be pointing to those instructions. If in a slot is -1, the native offset will be in the next slot
#define NATIVE_INDEX_IN_NEXT_SLOT 0xFFFF

//extern __thread struct vm_thread * self_thread;

#define READ_BYTECODE(jit_compiler) (*(jit_compiler)->pc++)
#define READ_U16(jit_compiler) \
    ((jit_compiler)->pc += 2, (uint16_t)(((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_CONSTANT(jit_compiler) (jit_compiler->function_to_compile->chunk.constants.values[READ_BYTECODE(jit_compiler)])
#define CURRENT_BYTECODE_INDEX(jit_compiler) (jit_compiler->pc - jit_compiler->function_to_compile->chunk.code)

static struct jit_compiler init_jit_compiler(struct function_object * function);
static void add(struct jit_compiler *);
static void sub(struct jit_compiler *);
static void lox_value(struct jit_compiler *, lox_value_t value, int bytecode_instruction_length);
static void comparation(struct jit_compiler *, op_code comparation_opcode, int bytecode_instruction_length);
static void greater(struct jit_compiler *);
static void negate(struct jit_compiler *);
static void multiplication(struct jit_compiler *);
static void division(struct jit_compiler *);
static void loop(struct jit_compiler *, uint16_t bytecode_backward_jump);
static void constant(struct jit_compiler *);
static void jump(struct jit_compiler *, uint16_t offset);
static void jump_if_false(struct jit_compiler *, uint16_t jump_offset);
static void print(struct jit_compiler *);
static void pop(struct jit_compiler *);
static void get_local(struct jit_compiler *);
static void set_local(struct jit_compiler *);
static void not(struct jit_compiler *);
static void enter_package(struct jit_compiler *);
static void exit_package(struct jit_compiler *);
static void get_global(struct jit_compiler *);
static void set_global(struct jit_compiler *);
static void define_global(struct jit_compiler *);
static void package_const(struct jit_compiler *);
static void initialize_struct(struct jit_compiler *);
static void get_struct_field(struct jit_compiler *);
static void set_struct_field(struct jit_compiler *);
static void nop(struct jit_compiler *);
static void initialize_array(struct jit_compiler *);
static void get_array_element(struct jit_compiler *);
static void set_array_element(struct jit_compiler *);
static void enter_monitor_jit(struct jit_compiler *);
static void exti_monitor_jit(struct jit_compiler *);
static void return_jit(struct jit_compiler *);
static void call(struct jit_compiler *);

static void record_pending_jump_to_patch(struct jit_compiler *, uint16_t jump_instruction_index,
        uint16_t bytecode_offset, uint16_t x64_jump_instruction_body_length);
static void record_compiled_bytecode(struct jit_compiler *, uint16_t native_compiled_index, int bytecode_instruction_length);
static uint16_t get_compiled_native_index_by_bytecode_index(struct jit_compiler *, uint16_t current_bytecode_index);
static void check_pending_jumps_to_patch(struct jit_compiler *, int bytecode_instruction_length);
static void cast_to_lox_boolean(struct jit_compiler *, register_t register_boolean_value);
static void set_al_with_cmp_result(struct jit_compiler *, op_code comparation_opcode);
static void number_const(struct jit_compiler *, int value, int instruction_length);
static void free_jit_compiler(struct jit_compiler *);
static uint16_t cast_lox_object_to_ptr(struct jit_compiler *, register_t lox_object_ptr);
static uint16_t cast_ptr_to_lox_object(struct jit_compiler *, register_t lox_object_ptr);
static void update_monitors_entered_array(struct jit_compiler *, register_t register_call_frame, int64_t monitor_to_enter);
static void update_last_monitor_entered_count(struct jit_compiler *, register_t register_call_frame, bool increase);
static void exit_monitor_jit(struct jit_compiler *);
static void read_last_monitor_entered(struct jit_compiler *, register_t register_call_frame_addr, struct function_object * function);
static uint16_t call_safepoint(struct jit_compiler *);
static uint16_t call_add_object_to_heap(struct jit_compiler *, register_t);
static uint16_t emit_lox_push(struct jit_compiler *, register_t);
static uint16_t emit_lox_pop(struct jit_compiler *, register_t);
static uint16_t emit_increase_lox_stack(struct jit_compiler *, int);
static uint16_t emit_decrease_lox_tsack(struct jit_compiler *, int);
static void emit_native_call(struct jit_compiler *, register_t function_object_addr_reg, int n_args);
static void switch_jit_to_vm_mode(struct jit_compiler *);

void * alloc_jit_runtime_info_arch() {
    return malloc(sizeof(struct x64_jit_runtime_info));
}

//TODO Add runtime information optimization
static void call(struct jit_compiler * jit_compiler) {
    int n_args = READ_BYTECODE(jit_compiler);
    bool is_parallel = READ_BYTECODE(jit_compiler);
    register_t function_register = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = cast_ptr_to_lox_object(jit_compiler, function_register);

    //Pop function_register
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_cmp(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_register),
             IMMEDIATE_TO_OPERAND(OBJ_NATIVE));

    emit_native_call(jit_compiler, function_register, n_args);

    call_safepoint(jit_compiler);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_CALL_LENGTH);
}

struct jit_compilation_result jit_compile_arch(struct function_object * function) {
    struct jit_compiler jit_compiler = init_jit_compiler(function);
    bool finish_compilation_flag = false;

    emit_prologue_x64_stack(&jit_compiler.native_compiled_code, function);
    setup_vm_to_jit_mode(&jit_compiler);

#ifndef VM_TEST
    push_stack_list(&jit_compiler.package_stack, self_thread->current_package);
#endif
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
            case OP_SET_GLOBAL: set_global(&jit_compiler); break;
            case OP_DEFINE_GLOBAL: define_global(&jit_compiler); break;
            case OP_ENTER_PACKAGE: enter_package(&jit_compiler); break;
            case OP_EXIT_PACKAGE: exit_package(&jit_compiler); break;
            case OP_PACKAGE_CONST: package_const(&jit_compiler); break;
            case OP_INITIALIZE_STRUCT: initialize_struct(&jit_compiler); break;
            case OP_GET_STRUCT_FIELD: get_struct_field(&jit_compiler); break;
            case OP_SET_STRUCT_FIELD: set_struct_field(&jit_compiler); break;
            case OP_NO_OP: nop(&jit_compiler); break;
            case OP_INITIALIZE_ARRAY: initialize_array(&jit_compiler); break;
            case OP_GET_ARRAY_ELEMENT: get_array_element(&jit_compiler); break;
            case OP_SET_ARRAY_ELEMENT: set_array_element(&jit_compiler); break;
            case OP_ENTER_MONITOR: enter_monitor_jit(&jit_compiler); break;
            case OP_EXIT_MONITOR: exit_monitor_jit(&jit_compiler); break;
            case OP_RETURN: return_jit(&jit_compiler); break;
            case OP_CALL: call(&jit_compiler); break;
            default: runtime_panic("Unhandled bytecode to compile %u\n", *(--jit_compiler.pc));
        }

        if(finish_compilation_flag){
            break;
        }
    }

    emit_epilogue_x64_stack(&jit_compiler.native_compiled_code, function);

    free_jit_compiler(&jit_compiler);

    return (struct jit_compilation_result) {
            .compiled_code = jit_compiler.native_compiled_code,
            .success = true,
    };
}

static void emit_native_call(struct jit_compiler * jit_compiler, register_t function_object_addr_reg, int n_args) {
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_object_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct native_object, native_fn)));

    for(int i = n_args - 1; i >= 0; i--){
        register_t arg_reg = peek_at_register_allocator(&jit_compiler->register_allocator, i);
        emit_lox_push(jit_compiler, arg_reg);
    }
    //Pop args registers
    for(int i = n_args - 1; i >= 0; i--){
        pop_register_allocator(&jit_compiler->register_allocator);
    }

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(RSP),
             IMMEDIATE_TO_OPERAND(n_args));

    register_t start_args_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(start_args_addr_reg),
             RSP_REGISTER_OPERAND);

    call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            REGISTER_TO_OPERAND(function_object_addr_reg),
            2,
            IMMEDIATE_TO_OPERAND(n_args),
            REGISTER_TO_OPERAND(start_args_addr_reg)
    );

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(start_args_addr_reg),
             RAX_REGISTER_OPERAND);
}

static void return_jit(struct jit_compiler * jit_compiler) {
    register_t returned_value = peek_register_allocator(&jit_compiler->register_allocator);
    bool some_value_returned = returned_value <= R15;

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(restore_prev_call_frame),
            0);

    emit_mov(&jit_compiler->native_compiled_code,
             RSP_REGISTER_OPERAND,
             RBP_REGISTER_OPERAND);

    emit_lox_push(jit_compiler, returned_value);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_RETURN_LENGTH);
}

static void exit_monitor_jit(struct jit_compiler * jit_compiler) {
    register_t current_frame_addr_reg_a = push_register_allocator(&jit_compiler->register_allocator);
    register_t current_frame_addr_reg_b = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_current_frame_vm_thread),
            1,
            REGISTER_TO_OPERAND(SELF_THREAD_ADDR_REG)
    );

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(current_frame_addr_reg_a),
             RAX_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(current_frame_addr_reg_b),
             RAX_REGISTER_OPERAND);

    update_last_monitor_entered_count(jit_compiler, current_frame_addr_reg_a, DECREASE_MONITOR_COUNT);

    read_last_monitor_entered(jit_compiler, current_frame_addr_reg_b, jit_compiler->function_to_compile);

    call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(exit_monitor),
            1,
            REGISTER_TO_OPERAND(current_frame_addr_reg_b));

    //Deallocate current_frame_addr_reg_a & current_frame_addr_reg_b
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_EXIT_MONITOR_LENGTH);
}

//Puts latest monitor ptr entered into register_call_frame_addr
//Equivalent in C: &function->monitors[call_frame->monitors_entered[call_frame->last_monitor_entered_index]]
//Code:
/**
 * r15 => register_call_frame_addr
 * mov r14, [r15 + offset(struct call_frame, last_monitor_entered_index)
 *
 * add r15, offset(struct call_frame, monitors_entered)
 * add r15, r14
 * mov r14, [r15]
 *
 * mov r13, function
 * add r13, offset(struct function_object, monitors)
 * add r13, r14
 *
 * mov register_call_frame_addr, r13
 */
static void read_last_monitor_entered(struct jit_compiler * jit_compiler,
        register_t register_call_frame_addr,
        struct function_object * function) {
    register_t last_monitor_index_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(last_monitor_index_reg),
             DISPLACEMENT_TO_OPERAND(register_call_frame_addr, offsetof(struct call_frame, last_monitor_entered_index)));

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_call_frame_addr),
             IMMEDIATE_TO_OPERAND(offsetof(struct call_frame, monitors_entered)));
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_call_frame_addr),
             REGISTER_TO_OPERAND(last_monitor_index_reg));
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(last_monitor_index_reg),
             DISPLACEMENT_TO_OPERAND(register_call_frame_addr, 0));

    register_t function_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_addr_reg),
             IMMEDIATE_TO_OPERAND((uint64_t) function));

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct function_object, monitors)));
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_addr_reg),
             REGISTER_TO_OPERAND(last_monitor_index_reg));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_call_frame_addr),
             REGISTER_TO_OPERAND(function_addr_reg));

    //pop last_monitor_index_reg & function_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);
}

// C equivalent: call_frame->monitors_entered[call_frame->last_monitor_entered_index++] = monitor_number_to_enter;
// mov rax, get_current_frame_vm_thread()
// mov r15, rax
// mov r14, rax 
//
// add r15, offset(struct callframe, monitors_entered)
// add r15, [r15 + offset(struct callframe, last_monitor_entered_index)] 
// mov [r15], monitor_number_to_enter
//
// add r14, offset(struct call_frame, last_monitor_entered_index)
// mov r13, [r14]
// inc r13
// mov [r14 + offset(struct callframe, monitors_entered)], r14
static void enter_monitor_jit(struct jit_compiler * jit_compiler) {
    int monitor_number_to_enter = READ_BYTECODE(jit_compiler);
    struct monitor * monitor_to_enter = &jit_compiler->function_to_compile->monitors[monitor_number_to_enter];
    
    register_t current_frame_addr_reg_a = push_register_allocator(&jit_compiler->register_allocator);
    register_t current_frame_addr_reg_b = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_current_frame_vm_thread),
            1,
            REGISTER_TO_OPERAND(SELF_THREAD_ADDR_REG)
    );

    emit_mov(&jit_compiler->native_compiled_code, 
        REGISTER_TO_OPERAND(current_frame_addr_reg_a), 
        RAX_REGISTER_OPERAND);
    
    emit_mov(&jit_compiler->native_compiled_code, 
        REGISTER_TO_OPERAND(current_frame_addr_reg_b), 
        RAX_REGISTER_OPERAND);

    update_monitors_entered_array(jit_compiler, current_frame_addr_reg_a, monitor_number_to_enter);
    update_last_monitor_entered_count(jit_compiler, current_frame_addr_reg_b, INCREASE_MONITOR_COUNT);
    
    //Deallocate current_frame_addr_reg_a & current_frame_addr_reg_b
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    //set_self_thread_waiting calls safepoint that run in vm mode
    //(another thread might be doing gc, so it will need to read the most up-to-date lox stack)
    call_external_c_function(
            jit_compiler,
            MODE_VM,
            KEEP_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(set_self_thread_waiting),
            0);

    call_external_c_function(
            jit_compiler,
            MODE_VM,
            KEEP_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(enter_monitor),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) monitor_to_enter));

    call_external_c_function(
            jit_compiler,
            MODE_VM,
            KEEP_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(set_self_thread_runnable),
            0);

    switch_vm_to_jit_mode(jit_compiler);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_ENTER_MONITOR_LENGTH);
}

// call_frame->monitors_entered[call_frame->last_monitor_entered_index] = monitor_number_to_enter
// add r15, offset(struct callframe, monitors_entered)
// add r15, [r15 + offset(struct callframe, last_monitor_entered_index)] 
// mov [r15], monitor_number_to_enter
static void update_monitors_entered_array(struct jit_compiler * jit_compiler, register_t register_call_frame, int64_t monitor_to_enter) {
    emit_add(&jit_compiler->native_compiled_code, 
        REGISTER_TO_OPERAND(register_call_frame), 
        IMMEDIATE_TO_OPERAND(offsetof(struct call_frame, monitors_entered)));

    emit_add(&jit_compiler->native_compiled_code,
        REGISTER_TO_OPERAND(register_call_frame),
        DISPLACEMENT_TO_OPERAND(register_call_frame, offsetof(struct call_frame, last_monitor_entered_index)));

    emit_mov(&jit_compiler->native_compiled_code,
        DISPLACEMENT_TO_OPERAND(register_call_frame, 0),
        IMMEDIATE_TO_OPERAND(monitor_to_enter));
}

// call_frame->last_monitor_entered_index++
// add r14, offset (struct call_frame, last_monitor_entered_index)
// mov r13, [r14]
// inc/dec r13
// mov [r14], r13
static void update_last_monitor_entered_count(struct jit_compiler * jit_compiler, register_t register_call_frame, bool increase) {
    emit_add(&jit_compiler->native_compiled_code,
        REGISTER_TO_OPERAND(register_call_frame),
        IMMEDIATE_TO_OPERAND(offsetof(struct call_frame, last_monitor_entered_index)));

    register_t count_register_value = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code, 
        REGISTER_TO_OPERAND(count_register_value), 
        DISPLACEMENT_TO_OPERAND(count_register_value, 0));    

    if(increase){
        emit_inc(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(count_register_value));
    } else {
        emit_dec(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(count_register_value));
    }

    emit_mov(&jit_compiler->native_compiled_code, 
        DISPLACEMENT_TO_OPERAND(register_call_frame, 0), 
        REGISTER_TO_OPERAND(count_register_value));

    pop_register_allocator(&jit_compiler->register_allocator);
}

static void set_array_element(struct jit_compiler * jit_compiler) {
    uint16_t array_index = READ_U16(jit_compiler);

    register_t element_addr_reg = peek_register_allocator(&jit_compiler->register_allocator);
    register_t new_element = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = cast_lox_object_to_ptr(jit_compiler, element_addr_reg);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(element_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values)));

    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(element_addr_reg, array_index * sizeof(lox_value_t)),
             REGISTER_TO_OPERAND(new_element));

    //pop element_addr_reg & new_element
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_SET_ARRAY_ELEMENT_LENGTH);
}

static void get_array_element(struct jit_compiler * jit_compiler) {
    uint16_t array_index = READ_U16(jit_compiler);

    register_t element_addr_reg = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = cast_lox_object_to_ptr(jit_compiler, element_addr_reg);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(element_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values)));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(element_addr_reg),
             DISPLACEMENT_TO_OPERAND(element_addr_reg, array_index * sizeof(lox_value_t)));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_ARRAY_ELEMENT_LENGTH);
}

static void initialize_array(struct jit_compiler * jit_compiler) {
    int n_elements = READ_U16(jit_compiler);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(alloc_array_object),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) n_elements));

    if(n_elements > 0){
        emit_lox_push(jit_compiler, RAX);
    }

    for(int i = 0; i < n_elements; i++){
        register_t array_value_reg = pop_register_allocator(&jit_compiler->register_allocator);
        int index_array_value = n_elements - 1 - i;

        call_external_c_function(
                jit_compiler,
                MODE_NATIVE,
                SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
                FUNCTION_TO_OPERAND(set_element_array),
                3,
                RAX_REGISTER_OPERAND,
                IMMEDIATE_TO_OPERAND(index_array_value),
                REGISTER_TO_OPERAND(array_value_reg));

        emit_lox_pop(jit_compiler, RAX);
    }

    register_t register_array_address = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_array_address),
             RAX_REGISTER_OPERAND);

    call_add_object_to_heap(jit_compiler, register_array_address);

    cast_ptr_to_lox_object(jit_compiler, register_array_address);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_INITIALIZE_ARRAY_LENGTH);
}

static void set_struct_field(struct jit_compiler * jit_compiler) {
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    register_t new_value_reg = peek_at_register_allocator(&jit_compiler->register_allocator, 0);
    register_t struct_instance_addr_reg = peek_at_register_allocator(&jit_compiler->register_allocator, 1);

    uint16_t first_instruction_index = cast_lox_object_to_ptr(jit_compiler, struct_instance_addr_reg);

    //Deallocate new_value_reg & struct_instance_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_instance_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(put_hash_table),
            3,
            REGISTER_TO_OPERAND(struct_instance_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            REGISTER_TO_OPERAND(new_value_reg));

    record_compiled_bytecode(jit_compiler, first_instruction_index, OP_SET_STRUCT_FIELD_LENGTH);
}

static void get_struct_field(struct jit_compiler * jit_compiler) {
    register_t struct_instance_addr_reg = peek_register_allocator(&jit_compiler->register_allocator);
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));

    uint16_t first_instruction_index = cast_lox_object_to_ptr(jit_compiler, struct_instance_addr_reg);

    //pop struct_instance_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_instance_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    //The value will be allocated rigth after RSP. RSP always points to the first non-used slot of the stack
    call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_hash_table),
            3,
            REGISTER_TO_OPERAND(struct_instance_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            REGISTER_TO_OPERAND(RSP));

    register_t field_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(field_value_reg),
             DISPLACEMENT_TO_OPERAND(RSP, 0));

    record_compiled_bytecode(jit_compiler, first_instruction_index, OP_GET_STRUCT_FIELD_LENGTH);
}

static void initialize_struct(struct jit_compiler * jit_compiler) {
    struct struct_definition_object * struct_definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    int n_fields = struct_definition->n_fields;

    uint16_t first_instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(alloc_struct_instance_object),
            0
    );

    emit_add(&jit_compiler->native_compiled_code,
             RAX_REGISTER_OPERAND,
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    //RAX will get overrided by call_external_c_function because it is where the return address will be stored
    emit_lox_push(jit_compiler, RAX);

    //structs will always have to contain atleast one argument, so this for loop will get executed -> RAX will get popped
    for(int i = 0; i < n_fields; i++) {
        struct string_object * field_name = struct_definition->field_names[struct_definition->n_fields - i - 1];
        register_t struct_field_value_reg = pop_register_allocator(&jit_compiler->register_allocator);

        call_external_c_function(
                jit_compiler,
                MODE_NATIVE,
                SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
                FUNCTION_TO_OPERAND(put_hash_table),
                3,
                RAX_REGISTER_OPERAND,
                IMMEDIATE_TO_OPERAND((uint64_t) field_name),
                REGISTER_TO_OPERAND(struct_field_value_reg)
        );

        //So that we can reuse it in the next call_external_c_function
        emit_lox_pop(jit_compiler, RAX);
    }

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(RAX),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));
    
    //Store struct instance address
    register_t register_to_store_address = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_to_store_address),
             RAX_REGISTER_OPERAND);

    call_add_object_to_heap(jit_compiler, register_to_store_address);

    cast_ptr_to_lox_object(jit_compiler, register_to_store_address);

    record_compiled_bytecode(jit_compiler, first_instruction_index, OP_INITIALIZE_STRUCT_LENGTH);
}

static void nop(struct jit_compiler * jit_compiler) {
    uint16_t instruction_index = emit_nop(&jit_compiler->native_compiled_code);
    record_compiled_bytecode(jit_compiler, instruction_index, OP_NO_OP_LENGTH);
}

static void package_const(struct jit_compiler * jit_compiler) {
    lox_value_t package_object = READ_CONSTANT(jit_compiler);
    struct package * package = ((struct package_object *) AS_OBJECT(package_object))->package;

    push_stack_list(&jit_compiler->package_stack, package);

    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_PACKAGE_CONST_LENGTH);
}

static void define_global(struct jit_compiler * jit_compiler) {
    struct string_object * global_name = AS_STRING_OBJECT(READ_CONSTANT(jit_compiler));
    struct package * current_package = peek_stack_list(&jit_compiler->package_stack);

    register_t new_global_value = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(put_hash_table),
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) global_name),
            REGISTER_TO_OPERAND(new_global_value)
    );

    call_safepoint(jit_compiler);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_DEFINE_GLOBAL_LENGTH);
}

static void enter_package(struct jit_compiler * jit_compiler) {
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_ENTER_PACKAGE_LENGTH);
}

static void exit_package(struct jit_compiler * jit_compiler) {
    pop_stack_list(&jit_compiler->package_stack);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_EXIT_PACKAGE_LENGTH);
}

static void set_global(struct jit_compiler * jit_compiler) {
    struct string_object * global_name = AS_STRING_OBJECT(READ_CONSTANT(jit_compiler));
    struct package * current_package = peek_stack_list(&jit_compiler->package_stack);

    register_t new_global_value = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(put_hash_table),
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) global_name),
            REGISTER_TO_OPERAND(new_global_value)
    );

    record_compiled_bytecode(jit_compiler, instruction_index, OP_SET_GLOBAL_LENGTH);
}

static void get_global(struct jit_compiler * jit_compiler) {
    struct string_object * name = AS_STRING_OBJECT(READ_CONSTANT(jit_compiler));
    struct package * current_package = peek_stack_list(&jit_compiler->package_stack);

    //The value will be allocated rigth after RSP
    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_hash_table),
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) name),
            REGISTER_TO_OPERAND(RSP)
    );

    register_t global_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(global_value_reg),
             DISPLACEMENT_TO_OPERAND(RSP, 0));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_GLOBAL_LENGTH);
}

//True value:  0x7ffc000000000003
//False value: 0x7ffc000000000002
//value1 = value0 + ((TRUE - value0) + (FALSE - value0))
//value1 = - value0 + TRUE + FALSE
//value1 = (TRUE - value) + FALSE
static void not(struct jit_compiler * jit_compiler) {
    register_t value_to_negate = peek_register_allocator(&jit_compiler->register_allocator);
    register_t lox_boolean_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(lox_boolean_value_reg),
             IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(value_to_negate),
             REGISTER_TO_OPERAND(lox_boolean_value_reg));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(lox_boolean_value_reg),
             IMMEDIATE_TO_OPERAND(FALSE_VALUE));

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(value_to_negate),
             REGISTER_TO_OPERAND(lox_boolean_value_reg));

    //pop lox_boolean_value_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_NOT_LENGTH);
}

static void set_local(struct jit_compiler * jit_compiler) {
    uint8_t slot = READ_BYTECODE(jit_compiler);

    if(slot > jit_compiler->last_stack_slot_allocated){
        uint8_t n_locals_to_grow = slot - jit_compiler->last_stack_slot_allocated;
        emit_increase_lox_stack(jit_compiler, slot - jit_compiler->last_stack_slot_allocated);
    }

    register_t register_local_value = peek_register_allocator(&jit_compiler->register_allocator);
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
             DISPLACEMENT_TO_OPERAND(RBP, offset_local_from_rbp));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_LOCAL_LENGTH);
}

static void print(struct jit_compiler * jit_compiler) {
    register_t to_print_register_arg = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_NATIVE,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(print_lox_value),
            1,
            REGISTER_TO_OPERAND(to_print_register_arg));

    //pop to_print_register_arg
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_PRINT_LENGTH);
}

static void jump_if_false(struct jit_compiler * jit_compiler, uint16_t jump_offset) {
    uint16_t instruction_index = call_safepoint(jit_compiler);

    register_t lox_boolean_value = peek_register_allocator(&jit_compiler->register_allocator);
    register_t register_true_lox_value = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_true_lox_value),
             IMMEDIATE_TO_OPERAND(TRUE_VALUE));

    emit_cmp(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(lox_boolean_value),
             REGISTER_TO_OPERAND(register_true_lox_value));

    uint16_t jmp_index = emit_near_jne(&jit_compiler->native_compiled_code, 0);

    //deallocate lox_boolean_value & register_true_lox_value
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    record_pending_jump_to_patch(jit_compiler, jmp_index, jump_offset, 2); //JNE takes two bytes as opcode
    record_compiled_bytecode(jit_compiler, instruction_index, OP_JUMP_IF_FALSE_LENGTH);
}

static void jump(struct jit_compiler * jit_compiler, uint16_t offset) {
    uint16_t instruction_index = call_safepoint(jit_compiler);

    uint16_t jmp_index = emit_near_jmp(&jit_compiler->native_compiled_code, 0); //We don't know the offset of where to jump

    record_pending_jump_to_patch(jit_compiler, jmp_index, offset, 1); //jmp takes only 1 byte as opcode
    record_compiled_bytecode(jit_compiler, instruction_index, OP_JUMP_LENGTH);
}

static void pop(struct jit_compiler * jit_compiler) {
    pop_register_allocator(&jit_compiler->register_allocator);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_POP_LENGTH);
}

static void loop(struct jit_compiler * jit_compiler, uint16_t bytecode_backward_jump) {
    uint16_t instruction_index = call_safepoint(jit_compiler);

    uint16_t bytecode_index_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_backward_jump;
    uint16_t native_index_to_jump = jit_compiler->compiled_bytecode_to_native_by_index[bytecode_index_to_jump];
    uint16_t current_native_index = jit_compiler->native_compiled_code.in_use;

    // +5 because of instruction size
    uint16_t jmp_offset = (current_native_index - native_index_to_jump) + 5;

    //Loop bytecode instruction always jumps backward
    emit_near_jmp(&jit_compiler->native_compiled_code, -((int) jmp_offset));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_LOOP_LENGTH);
}

static void constant(struct jit_compiler * jit_compiler) {
    lox_value_t constant = READ_CONSTANT(jit_compiler);

    register_t register_to_store_constant = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(register_to_store_constant),
                                  IMMEDIATE_TO_OPERAND(constant));

    record_compiled_bytecode(jit_compiler, mov_index, OP_CONSTANT_LENGTH);
}

// a / b
static void division(struct jit_compiler * jit_compiler) {
    register_t b = pop_register_allocator(&jit_compiler->register_allocator);
    register_t a = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  RAX_REGISTER_OPERAND,
                                  REGISTER_TO_OPERAND(a));

    emit_idiv(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(b));

    register_t result_register = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(result_register),
             RAX_REGISTER_OPERAND);

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
    uint16_t native_offset = emit_cmp(&jit_compiler->native_compiled_code,
                                      REGISTER_TO_OPERAND(a),
                                      REGISTER_TO_OPERAND(b));

    set_al_with_cmp_result(jit_compiler, comparation_opcode);

    register_t register_casted_value = push_register_allocator(&jit_compiler->register_allocator);

    cast_to_lox_boolean(jit_compiler, register_casted_value);

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

    register_t register_quiet_float_nan = push_register_allocator(&jit_compiler->register_allocator);

    //In x64 the max size of the immediate is 32 bit, FLOAT_QNAN is 64 bit.
    //We need to store FLOAT_QNAN in 64 bit register to later be able to or with register_casted_value
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_quiet_float_nan),
             IMMEDIATE_TO_OPERAND(FLOAT_QNAN));

    emit_or(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(register_casted_value),
            REGISTER_TO_OPERAND(register_quiet_float_nan));

    //Dellacate register_quiet_float_nan
    pop_register_allocator(&jit_compiler->register_allocator);
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

//TODO Call lox addition
static void add(struct jit_compiler * jit_compiler) {
    register_t operandB = pop_register_allocator(&jit_compiler->register_allocator);
    register_t operandA = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t add_index = emit_add(&jit_compiler->native_compiled_code,
                                  REGISTER_TO_OPERAND(operandA),
                                  REGISTER_TO_OPERAND(operandB));

    push_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, add_index, OP_ADD_LENGTH);
}

static void record_pending_jump_to_patch(struct jit_compiler * jit_compiler, uint16_t jump_instruction_index,
        uint16_t bytecode_offset,
        uint16_t x64_jump_instruction_body_length) {
    uint16_t bytecode_instruction_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) + bytecode_offset;

    if(jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump] == NULL) {
        jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump] = alloc_pending_path_jump();
    }

    //Near jump instruction: (opcode 2 byte) + (offset 4 bytes) TODO check return value
    add_pending_path_jump(jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump], jump_instruction_index + x64_jump_instruction_body_length);
}

static struct jit_compiler init_jit_compiler(struct function_object * function) {
    struct jit_compiler compiler;

    compiler.current_mode = MODE_JIT;
    compiler.compiled_bytecode_to_native_by_index = malloc(sizeof(uint16_t) * function->chunk.in_use);
    memset(compiler.compiled_bytecode_to_native_by_index, 0, sizeof(uint16_t) * function->chunk.in_use);

    compiler.pending_jumps_to_patch = malloc(sizeof(void *) * function->chunk.in_use);
    memset(compiler.pending_jumps_to_patch, 0, sizeof(void *) * function->chunk.in_use);

    compiler.last_stack_slot_allocated = function->n_arguments > 0 ? function->n_arguments : -1;
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

//Assembly implementation of AS_OBJECT defined in types.h
//AS_OBJECT(value) ((struct object *) (uintptr_t)((value) & ~(FLOAT_SIGN_BIT | FLOAT_QNAN)))
//Allocates & deallocates new register
static uint16_t cast_lox_object_to_ptr(struct jit_compiler * jit_compiler, register_t lox_object_ptr) {
    //~(FLOAT_SIGN_BIT | FLOAT_QNAN)
    register_t not_fsb_qfn_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(not_fsb_qfn_reg),
             IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | FLOAT_QNAN)));

    emit_and(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(lox_object_ptr),
             REGISTER_TO_OPERAND(not_fsb_qfn_reg));

    pop_register_allocator(&jit_compiler->register_allocator);

    return instruction_index;
}

//Assembly implementation of TO_LOX_VALUE_OBJECT defined in types.h
//TO_LOX_VALUE_OBJECT(value) (FLOAT_SIGN_BIT | FLOAT_QNAN | (lox_value_t) value)
//Allocates & deallocates new register
static uint16_t cast_ptr_to_lox_object(struct jit_compiler * jit_compiler, register_t lox_object_ptr) {
    //FLOAT_SIGN_BIT | FLOAT_QNAN
    register_t or_fsb_qfn_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(or_fsb_qfn_reg),
             IMMEDIATE_TO_OPERAND(FLOAT_SIGN_BIT | FLOAT_QNAN));

    emit_or(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(lox_object_ptr),
            REGISTER_TO_OPERAND(or_fsb_qfn_reg));

    pop_register_allocator(&jit_compiler->register_allocator);

    return instruction_index;
}

static void check_pending_jumps_to_patch(struct jit_compiler * jit_compiler, int bytecode_instruction_length) {
    uint16_t current_bytecode_index = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_instruction_length;
    uint16_t current_compiled_index = get_compiled_native_index_by_bytecode_index(jit_compiler, current_bytecode_index);

    struct pending_path_jump * pending_jumps = jit_compiler->pending_jumps_to_patch[current_bytecode_index];

    if(pending_jumps != NULL) {
        for(int i = 0; i < MAX_JUMPS_REFERENCES_TO_LINE; i++){
            uint16_t compiled_native_jmp_offset_index = pending_jumps->compiled_native_jmp_offset_index[i];

            if (compiled_native_jmp_offset_index != 0) {
                //compiled_native_jmp_offset_index points to native jmp offset part of the instruction
                //-4 to substract the jmp offset since it takes 4 bytes
                uint16_t native_jmp_offset = current_compiled_index - (compiled_native_jmp_offset_index + 4);
                uint16_t * compiled_native_jmp_offset_index_ptr = (uint16_t *) (jit_compiler->native_compiled_code.values + compiled_native_jmp_offset_index);

                *compiled_native_jmp_offset_index_ptr = native_jmp_offset;
            }
        }

        jit_compiler->pending_jumps_to_patch[current_bytecode_index] = NULL;
        free(pending_jumps);
    }
}

//Copies allocated registes to lox stack.
//It updates vm self-thread esp pointer and updates rsp native register
static void switch_jit_to_vm_mode(struct jit_compiler * jit_compiler) {
    int n_allocated_registers = jit_compiler->register_allocator.n_allocated_registers;

    if(n_allocated_registers == 0){
        return;
    }

    register_t esp_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    //Save esp vm address into esp_addr_reg
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(esp_addr_reg),
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    for(int i = n_allocated_registers - 1; i > 0; i--) {
        register_t stack_value_reg = peek_at_register_allocator(&jit_compiler->register_allocator, i);

        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(esp_addr_reg, 0),
                 REGISTER_TO_OPERAND(stack_value_reg));

        emit_add(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(esp_addr_reg),
                 IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));
    }

    emit_add(&jit_compiler->native_compiled_code,
             RSP_REGISTER_OPERAND,
             IMMEDIATE_TO_OPERAND(n_allocated_registers * sizeof(lox_value_t)));

    //Update updated esp vm value
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             REGISTER_TO_OPERAND(esp_addr_reg));

    //pop esp_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    //Restore previous rsp & rbp
    emit_mov(&jit_compiler->native_compiled_code, RSP_REGISTER_OPERAND, RCX_REGISTER_OPERAND);
    emit_mov(&jit_compiler->native_compiled_code, RBP_REGISTER_OPERAND, RDX_REGISTER_OPERAND);
}

//As the vm stack might have some data, we need to update vm esp, so that if any thread is garbage collecting,
//it can read the most up-to-date stack without discarding data
//This is only used here since the safepoint calls are done in places where the stack is emtpy
//Note that the stack only contains values when an expression is being executed.
static uint16_t call_safepoint(struct jit_compiler * jit_compiler) {
    return call_external_c_function(
            jit_compiler,
            MODE_VM,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(check_gc_on_safe_point_alg),
            0);
}

static uint16_t call_add_object_to_heap(struct jit_compiler * jit_compiler, register_t object_addr_reg) {
    register_t lox_object_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    return call_external_c_function(
            jit_compiler,
            MODE_VM,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(add_object_to_heap),
            1,
            REGISTER_TO_OPERAND(object_addr_reg));
}

//Se x64_stack.h
//Same as vm.c push_stack_vm
static uint16_t emit_lox_push(struct jit_compiler * jit_compiler, register_t reg) {
    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(RSP, 0),
             REGISTER_TO_OPERAND(reg));

    emit_increase_lox_stack(jit_compiler, 1);

    return instruction_index;
}

//Same as vm.c pop_stack_vm
static uint16_t emit_lox_pop(struct jit_compiler * jit_compiler, register_t reg) {
    uint16_t instruction_index = emit_decrease_lox_tsack(jit_compiler, 1);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(reg),
             DISPLACEMENT_TO_OPERAND(RSP, 0));

    return instruction_index;
}

static uint16_t emit_increase_lox_stack(struct jit_compiler * jit_compiler, int n_locals) {
    return emit_add(&jit_compiler->native_compiled_code,
                    RSP_REGISTER_OPERAND,
                    IMMEDIATE_TO_OPERAND(sizeof(lox_value_t) * n_locals));
}

static uint16_t emit_decrease_lox_tsack(struct jit_compiler * jit_compiler, int n_locals) {
    return emit_sub(&jit_compiler->native_compiled_code,
                    RSP_REGISTER_OPERAND,
                    IMMEDIATE_TO_OPERAND(sizeof(lox_value_t) * n_locals));
}

static uint16_t get_compiled_native_index_by_bytecode_index(struct jit_compiler * jit_compiler, uint16_t current_bytecode_index) {
    uint16_t * current = jit_compiler->compiled_bytecode_to_native_by_index + current_bytecode_index;

    for(; *current == NATIVE_INDEX_IN_NEXT_SLOT; current++);

    return *current;
}

static void free_jit_compiler(struct jit_compiler * jit_compiler) {
    for(int i = 0; i < jit_compiler->function_to_compile->chunk.in_use; i++){
        if(jit_compiler->pending_jumps_to_patch[i] != NULL){
            free(jit_compiler->pending_jumps_to_patch[i]);
        }
    }
    free_stack_list(&jit_compiler->package_stack);
    free(jit_compiler->pending_jumps_to_patch);
    free(jit_compiler->compiled_bytecode_to_native_by_index);
}