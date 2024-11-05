#include "x64_jit_compiler.h"

extern __thread struct vm_thread * self_thread;

extern struct struct_instance_object * alloc_struct_instance_gc_alg(struct struct_definition_object *);
extern struct array_object * alloc_array_gc_alg(int n_elements);
extern bool get_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t *value);
extern bool put_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);
extern struct call_frame * get_current_frame_vm_thread(struct vm_thread *);
extern bool set_element_array(struct array_object * array, int index, lox_value_t value);
extern void enter_monitor(struct monitor * monitor);
extern void exit_monitor(struct monitor * monitor);
extern void print_lox_value(lox_value_t value);
extern void runtime_panic(char * format, ...);
extern void check_gc_on_safe_point_alg();
extern void set_self_thread_runnable();
extern void set_self_thread_waiting();
extern bool restore_prev_call_frame();
extern struct jit_mode_switch_info switch_jit_to_vm_gc_mode(struct jit_compiler *);
extern struct jit_mode_switch_info switch_vm_gc_to_jit_mode(struct jit_compiler *, struct jit_mode_switch_info);
extern void switch_jit_to_native_mode(struct jit_compiler *);
extern struct binary_operation binary_operations[];
extern struct single_operation single_operations[];
extern struct jit_mode_switch_info setup_vm_to_jit_mode(struct jit_compiler *);
extern void call(lox_value_t callee_lox, int n_args, bool is_parallel);
extern lox_value_t addition_lox(lox_value_t a, lox_value_t b);
extern struct gc_barriers get_barriers_gc_alg();

void switch_native_to_jit_mode(struct jit_compiler *);

//Used by jit_compiler::compiled_bytecode_to_native_by_index Some operations are not compiled to native code, but some jumps bytecode offset
//will be pointing to those operations. If in a slot is -1, the native offset will be in the next slot
#define NATIVE_INDEX_IN_NEXT_SLOT 0xFFFF

//extern __thread struct vm_thread * self_thread;

#define READ_BYTECODE(jit_compiler) (*(jit_compiler)->pc++)
#define READ_U16(jit_compiler) \
    ((jit_compiler)->pc += 2, (uint16_t)(((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_U64(jit_compiler) \
    ((jit_compiler)->pc += 8, (uint64_t)(((jit_compiler)->pc[-8] << 54) | ((jit_compiler)->pc[-7] << 48) | ((jit_compiler)->pc[-6] << 40) | \
    ((jit_compiler)->pc[-5] << 32) | ((jit_compiler)->pc[-4] << 24) | ((jit_compiler)->pc[-3] << 16) | ((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_CONSTANT(jit_compiler) (jit_compiler->function_to_compile->chunk->constants.values[READ_BYTECODE(jit_compiler)])
#define CURRENT_BYTECODE_INDEX(jit_compiler) (jit_compiler->pc - jit_compiler->function_to_compile->chunk->code)

static struct jit_compiler init_jit_compiler(struct function_object * function);
static void lox_value(struct jit_compiler *, lox_value_t value, int bytecode_instruction_length);
static void greater(struct jit_compiler *);
static void loop(struct jit_compiler *, uint16_t bytecode_backward_jump);
static void constant(struct jit_compiler *);
static void jump(struct jit_compiler *, uint16_t offset);
static void jump_if_false(struct jit_compiler *, uint16_t jump_offset);
static void print(struct jit_compiler *);
static void pop(struct jit_compiler *);
static void get_local(struct jit_compiler *);
static void set_local(struct jit_compiler *);
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
static void exit_monitor_jit(struct jit_compiler *);
static void enter_monitor_explicit_jit(struct jit_compiler *);
static void exit_monitor_explicit_jit(struct jit_compiler *);
static uint16_t emit_do_enter_monitor(struct jit_compiler *, struct monitor *);
static void return_jit(struct jit_compiler *, bool * finish_compilation_flag);
static void call_jit(struct jit_compiler *);
static void eof(struct jit_compiler *, bool * finish_compilation_flag);

static void record_pending_jump_to_patch(struct jit_compiler *, uint16_t jump_instruction_index,
        uint16_t bytecode_offset, uint16_t x64_jump_instruction_body_length);
static void record_compiled_bytecode(struct jit_compiler *, uint16_t native_compiled_index, int bytecode_instruction_length);
static uint16_t get_compiled_native_index_by_bytecode_index(struct jit_compiler *, uint16_t current_bytecode_index);
static void check_pending_jumps_to_patch(struct jit_compiler *, int bytecode_instruction_length);
static void set_al_with_cmp_result(struct jit_compiler *, bytecode_t comparation_opcode);
static void number_const(struct jit_compiler *, int value, int instruction_length);
static void free_jit_compiler(struct jit_compiler *);
static uint16_t cast_lox_object_to_ptr(struct jit_compiler *, register_t lox_object_ptr);
static uint16_t cast_ptr_to_lox_object(struct jit_compiler *, register_t lox_object_ptr);
static uint16_t call_safepoint(struct jit_compiler *);
static uint16_t emit_lox_push(struct jit_compiler *, register_t);
static uint16_t emit_lox_pop(struct jit_compiler *, register_t);
static uint16_t emit_increase_lox_stack(struct jit_compiler *, int);
static uint16_t emit_decrease_lox_stack(struct jit_compiler *jit_compiler, int n_locals);
static struct pop_stack_operand_result pop_stack_operand_jit_stack(struct jit_compiler *);
static struct pop_stack_operand_result pop_stack_operand_jit_stack_as_register(struct jit_compiler *);
static void record_multiple_compiled_bytecode(struct jit_compiler * jit_compiler,
        int bytecode_instruction_length,int n_instrucion_indexes, ...);
static void binary_operation(struct jit_compiler * jit_compiler, int instruction_length, bytecode_t instruction);
static void single_operation(struct jit_compiler * jit_compiler, int instruction_length, bytecode_t instruction);
static uint16_t find_native_index_by_compiled_bytecode(struct jit_compiler *, uint16_t bytecode_index);
static uint16_t load_arguments(struct jit_compiler * jit_compiler, int n_arguments);
static bool does_single_bytecode_instruction(bytecode_t opcode);
static void wrapper_write_struct_field_barrier(struct object *, lox_value_t);
static void wrapper_write_array_element_barrier(struct object *, lox_value_t);

struct jit_compilation_result jit_compile_arch(struct function_object * function) {
    struct jit_compiler jit_compiler = init_jit_compiler(function);
    bool finish_compilation_flag = false;

    emit_prologue_x64_stack(&jit_compiler.native_compiled_code);
    setup_vm_to_jit_mode(&jit_compiler);

    push_stack_list(&jit_compiler.package_stack, self_thread->current_package);

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
            case OP_ADD: binary_operation(&jit_compiler, OP_ADD_LENGTH, OP_ADD); break;
            case OP_SUB: binary_operation(&jit_compiler, OP_SUB_LENGTH, OP_SUB); break;
            case OP_MUL: binary_operation(&jit_compiler, OP_MUL_LENGTH, OP_MUL); break;
            case OP_DIV: binary_operation(&jit_compiler, OP_DIV_LENGTH, OP_DIV); break;
            case OP_NEGATE: single_operation(&jit_compiler, OP_NEGATE_LENGTH, OP_NEGATE); break;
            case OP_POP: pop(&jit_compiler); break;
            case OP_TRUE: lox_value(&jit_compiler, TRUE_VALUE, OP_TRUE_LENGTH); break;
            case OP_FALSE: lox_value(&jit_compiler, FALSE_VALUE, OP_FALSE_LENGTH); break;
            case OP_NIL: lox_value(&jit_compiler, NIL_VALUE, OP_NIL_LENGTH); break;
            case OP_EQUAL: binary_operation(&jit_compiler, OP_EQUAL_LENGTH, OP_EQUAL); break;
            case OP_GREATER: binary_operation(&jit_compiler, OP_GREATER_LENGTH, OP_GREATER); break;
            case OP_LESS: binary_operation(&jit_compiler, OP_LESS_LENGTH, OP_LESS); break;
            case OP_GET_LOCAL: get_local(&jit_compiler); break;
            case OP_SET_LOCAL: set_local(&jit_compiler); break;
            case OP_JUMP_IF_FALSE: jump_if_false(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_JUMP: jump(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_LOOP: loop(&jit_compiler, READ_U16(&jit_compiler)); break;
            case OP_NOT: single_operation(&jit_compiler, OP_NOT_LENGTH, OP_NOT); break;
            case OP_EOF: eof(&jit_compiler, &finish_compilation_flag); break;
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
            case OP_ENTER_MONITOR_EXPLICIT: enter_monitor_explicit_jit(&jit_compiler); break;
            case OP_EXIT_MONITOR_EXPLICIT: exit_monitor_explicit_jit(&jit_compiler); break;
            case OP_ENTER_MONITOR: enter_monitor_jit(&jit_compiler); break;
            case OP_EXIT_MONITOR: exit_monitor_jit(&jit_compiler); break;
            case OP_RETURN: return_jit(&jit_compiler, &finish_compilation_flag); break;
            case OP_CALL: call_jit(&jit_compiler); break;
            default: runtime_panic("Unhandled bytecode to compile %u\n", *(--jit_compiler.pc));
        }

        if(finish_compilation_flag){
            break;
        }
    }

    emit_epilogue_x64_stack(&jit_compiler.native_compiled_code);

    free_jit_compiler(&jit_compiler);

    return (struct jit_compilation_result) {
            .compiled_code = jit_compiler.native_compiled_code,
            .success = true,
    };
}

static void call_jit(struct jit_compiler * jit_compiler) {
    int n_args = READ_BYTECODE(jit_compiler);
    bool is_parallel = READ_BYTECODE(jit_compiler);

    uint16_t instruction_index = load_arguments(jit_compiler, n_args);

    struct pop_stack_operand_result function_object = pop_stack_operand_jit_stack_as_register(jit_compiler);

    call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(call),
            3,
            REGISTER_TO_OPERAND(function_object.operand.as.reg),
            IMMEDIATE_TO_OPERAND(n_args),
            IMMEDIATE_TO_OPERAND(is_parallel));

    //call() modifies vm_thread->esp to point the correct value_node
    emit_mov(&jit_compiler->native_compiled_code,
             LOX_ESP_REG_OPERAND,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)));

    //pop function_object
    pop_register_allocator(&jit_compiler->register_allocator);

    register_t return_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code, 
            REGISTER_TO_OPERAND(return_value_reg),
            DISPLACEMENT_TO_OPERAND(LOX_ESP_REG, - sizeof(lox_value_t)));

    push_register_jit_stack(&jit_compiler->jit_stack, return_value_reg);

    record_compiled_bytecode(jit_compiler, PICK_FIRST_NOT_ZERO(instruction_index, function_object.instruction_index), OP_CALL_LENGTH);
}

static uint16_t load_arguments(struct jit_compiler * jit_compiler, int n_arguments) {
    if(n_arguments == 0){
        return 0;
    }

    uint16_t instruction_index = emit_add(&jit_compiler->native_compiled_code,
             LOX_ESP_REG_OPERAND,
             IMMEDIATE_TO_OPERAND(n_arguments * sizeof(lox_value_t)));

    for(int i = n_arguments - 1; i >= 0; i--){
        struct pop_stack_operand_result argument_pop_result = pop_stack_operand_jit_stack_as_register(jit_compiler);
        register_t argument_reg = argument_pop_result.operand.as.reg;

        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(LOX_ESP_REG, - ((i + 1) * sizeof(lox_value_t))),
                 REGISTER_TO_OPERAND(argument_reg));

        //pop argument_reg
        pop_register_allocator(&jit_compiler->register_allocator);
    }

    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             LOX_ESP_REG_OPERAND);

    return instruction_index;
}

static void eof(struct jit_compiler * jit_compiler, bool * finish_compilation_flag) {
    *finish_compilation_flag = true;

    record_compiled_bytecode(jit_compiler,
                             jit_compiler->native_compiled_code.in_use,
                             OP_EOF_LENGTH);
}

static void return_jit(struct jit_compiler * jit_compiler, bool * finish_compilation_flag) {
    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(restore_prev_call_frame),
            0);

    //Same as vm.c self_thread->esp = current_frame->slots
    emit_mov(&jit_compiler->native_compiled_code,
             LOX_ESP_REG_OPERAND,
             LOX_EBP_REG_OPERAND);

    //We push the returned vlaue
    struct pop_stack_operand_result returned_value = pop_stack_operand_jit_stack_as_register(jit_compiler);
    emit_lox_push(jit_compiler, returned_value.operand.as.reg);
    pop_register_allocator(&jit_compiler->register_allocator);

    //We update self_thread with the new esp value_node
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             LOX_ESP_REG_OPERAND);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_RETURN_LENGTH);

    //TODO REMOVE
    *finish_compilation_flag = true;
}

static void enter_monitor_explicit_jit(struct jit_compiler * jit_compiler) {
    struct monitor * monitor = (struct monitor *) READ_U64(jit_compiler);
    uint16_t instruction_index = emit_do_enter_monitor(jit_compiler, monitor);
    record_compiled_bytecode(jit_compiler, instruction_index, OP_ENTER_MONITOR_EXPLICIT_LENGTH);
}

static void enter_monitor_jit(struct jit_compiler * jit_compiler) {
    monitor_number_t monitor_number_to_enter = READ_BYTECODE(jit_compiler);
    struct monitor * monitor_to_enter = &jit_compiler->function_to_compile->monitors[monitor_number_to_enter];
    uint16_t instruction_index = emit_do_enter_monitor(jit_compiler, monitor_to_enter);
    record_compiled_bytecode(jit_compiler, instruction_index, OP_ENTER_MONITOR_LENGTH);
}

static uint16_t emit_do_enter_monitor(struct jit_compiler * jit_compiler, struct monitor * monitor) {
    //set_self_thread_waiting calls safepoint that run in vm mode
    //(another thread might be doing gc, so it will need to read the most up-to-date lox stack)
    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            KEEP_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(set_self_thread_waiting),
            0);

    call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            KEEP_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(enter_monitor),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) monitor));

    call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            KEEP_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(set_self_thread_runnable),
            0);

    int h_heap_allocations = n_heap_allocations_in_jit_stack(&jit_compiler->jit_stack);
    switch_vm_gc_to_jit_mode(jit_compiler, (struct jit_mode_switch_info) {h_heap_allocations});

    return instruction_index;
}

static void exit_monitor_jit(struct jit_compiler * jit_compiler) {
    monitor_number_t monitor_number_to_exit = READ_BYTECODE(jit_compiler);
    struct monitor * monitor = &jit_compiler->function_to_compile->monitors[monitor_number_to_exit];

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(exit_monitor),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) monitor)
    );

    record_compiled_bytecode(jit_compiler, instruction_index, OP_EXIT_MONITOR_LENGTH);
}

static void exit_monitor_explicit_jit(struct jit_compiler * jit_compiler) {
    struct monitor * monitor = (struct monitor *) READ_U64(jit_compiler);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(exit_monitor),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) monitor)
    );

    record_compiled_bytecode(jit_compiler, instruction_index, OP_EXIT_MONITOR_EXPLICIT_LENGTH);
}

static void set_array_element(struct jit_compiler * jit_compiler) {
    uint16_t array_index = READ_U16(jit_compiler);

    struct pop_stack_operand_result element_addr_pop = pop_stack_operand_jit_stack_as_register(jit_compiler);
    struct pop_stack_operand_result new_element_pop = pop_stack_operand_jit_stack_as_register(jit_compiler);
    register_t element_addr_reg = element_addr_pop.operand.as.reg;
    register_t new_element_reg = new_element_pop.operand.as.reg;

    uint16_t instruction_index_cast = cast_lox_object_to_ptr(jit_compiler, element_addr_reg);

    if (get_barriers_gc_alg().write_array_element != NULL) {
        call_external_c_function(
                jit_compiler,
                MODE_JIT,
                SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
                FUNCTION_TO_OPERAND(wrapper_write_array_element_barrier),
                2,
                REGISTER_TO_OPERAND(element_addr_reg),
                REGISTER_TO_OPERAND(new_element_reg)
        );
    }

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(element_addr_reg),
             IMMEDIATE_TO_OPERAND(
                     offsetof(struct array_object, values) +
                     offsetof(struct lox_arraylist, values)));

    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(element_addr_reg, array_index * sizeof(lox_value_t)),
             REGISTER_TO_OPERAND(new_element_reg));

    //pop element_addr_reg & new_element_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = PICK_FIRST_NOT_ZERO_3(element_addr_pop.instruction_index,
                                                       new_element_pop.instruction_index,
                                                       instruction_index_cast);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_SET_ARRAY_ELEMENT_LENGTH);
}

static void get_array_element(struct jit_compiler * jit_compiler) {
    uint16_t array_index = READ_U16(jit_compiler);

    struct pop_stack_operand_result array_addr_pop_result = pop_stack_operand_jit_stack_as_register(jit_compiler);
    register_t array_addr_reg = array_addr_pop_result.operand.as.reg;

    uint16_t instruction_index = cast_lox_object_to_ptr(jit_compiler, array_addr_reg);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(array_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct array_object, values)));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(array_addr_reg),
             DISPLACEMENT_TO_OPERAND(array_addr_reg, array_index * sizeof(lox_value_t)));

    push_register_jit_stack(&jit_compiler->jit_stack, array_addr_reg);

    record_compiled_bytecode(jit_compiler, PICK_FIRST_NOT_ZERO(array_addr_pop_result.instruction_index, instruction_index),
                             OP_GET_ARRAY_ELEMENT_LENGTH);
}

static void initialize_array(struct jit_compiler * jit_compiler) {
    int n_elements = READ_U16(jit_compiler);
    bool is_emtpy_initialization = READ_BYTECODE(jit_compiler);

    //Allocate array object & add to heap list
    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(alloc_array_gc_alg),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) n_elements));

    register_t array_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(array_addr_reg),
             RAX_REGISTER_OPERAND);

    //Load array object values into array_values_addr_reg
    register_t array_values_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(array_values_addr_reg),
             REGISTER_TO_OPERAND(array_addr_reg));
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(array_values_addr_reg),
             IMMEDIATE_TO_OPERAND(
                     offsetof(struct array_object, values) +
                     offsetof(struct lox_arraylist, values)));

    //Load array values from lox stack into array_values_addr_reg
    for(int i = 0; i < n_elements && !is_emtpy_initialization; i++){
        struct pop_stack_operand_result pop_result = pop_stack_operand_jit_stack_as_register(jit_compiler);
        size_t current_array_offset = (n_elements - i - 1) * sizeof(lox_value_t);

        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(array_values_addr_reg, current_array_offset),
                 pop_result.operand);

        pop_register_allocator(&jit_compiler->register_allocator);
    }

    register_t register_array_lox_addr_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_array_lox_addr_reg),
             REGISTER_TO_OPERAND(array_addr_reg));

    cast_ptr_to_lox_object(jit_compiler, register_array_lox_addr_reg);

    push_heap_allocated_register_jit_stack(&jit_compiler->jit_stack, register_array_lox_addr_reg);

    //Pop array_values_addr_reg & register_array_lox_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_INITIALIZE_ARRAY_LENGTH);
}

static void set_struct_field(struct jit_compiler * jit_compiler) {
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    struct pop_stack_operand_result new_value_pop_result = pop_stack_operand_jit_stack(jit_compiler);
    struct pop_stack_operand_result struct_addr_pop_result = pop_stack_operand_jit_stack_as_register(jit_compiler);
    register_t struct_addr_reg = struct_addr_pop_result.operand.as.reg;

    uint16_t first_instruction_index = cast_lox_object_to_ptr(jit_compiler, struct_addr_reg);

    if (get_barriers_gc_alg().write_struct_field != NULL) {
        call_external_c_function(
                jit_compiler,
                MODE_JIT,
                SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
                FUNCTION_TO_OPERAND(wrapper_write_struct_field_barrier),
                2,
                REGISTER_TO_OPERAND(struct_addr_reg),
                new_value_pop_result.operand
        );
    }

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(put_hash_table),
            3,
            REGISTER_TO_OPERAND(struct_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            new_value_pop_result.operand);

    //Deallocate new_value_reg & struct_instance_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    if(struct_addr_pop_result.operand.type == REGISTER_OPERAND){
        pop_register_allocator(&jit_compiler->register_allocator);
    }
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler,
        PICK_FIRST_NOT_ZERO(new_value_pop_result.instruction_index, struct_addr_pop_result.instruction_index),
        OP_SET_STRUCT_FIELD_LENGTH);
}

static void get_struct_field(struct jit_compiler * jit_compiler) {
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));

    //Load struct's field address into struct_addr_reg
    struct pop_stack_operand_result struct_addr_pop_result = pop_stack_operand_jit_stack_as_register(jit_compiler);
    uint16_t instruction_index = cast_lox_object_to_ptr(jit_compiler, struct_addr_pop_result.operand.as.reg);
    register_t struct_addr_reg = struct_addr_pop_result.operand.as.reg;
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    //Load field_value_reg which will hold the struct value_node
    register_t field_value_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(field_value_reg),
             LOX_ESP_REG_OPERAND);

    //The value_node will be allocated rigth after lox rsp. lox rsp always points to the first non-used slot of the stack
    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_hash_table),
            3,
            REGISTER_TO_OPERAND(struct_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            REGISTER_TO_OPERAND(field_value_reg));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(field_value_reg),
             DISPLACEMENT_TO_OPERAND(field_value_reg, 0));

    //pop struct_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(push_register_allocator(&jit_compiler->register_allocator)),
             REGISTER_TO_OPERAND(field_value_reg));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_STRUCT_FIELD_LENGTH);

    push_register_jit_stack(&jit_compiler->jit_stack, field_value_reg);
}

static void initialize_struct(struct jit_compiler * jit_compiler) {
    struct struct_definition_object * struct_definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    int n_fields = struct_definition->n_fields;

    //Alloc struct & load fields_nodes into struct_addr_reg
    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(alloc_struct_instance_gc_alg),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) struct_definition)
    );
    register_t struct_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             RAX_REGISTER_OPERAND);
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    //Load struct fields_nodes into lox stack
    for(int i = 0; i < n_fields; i++){
        struct pop_stack_operand_result pop_result = pop_stack_operand_jit_stack_as_register(jit_compiler);
        struct string_object * current_field_name = struct_definition->field_names[n_fields - i -  1];

        call_external_c_function(
                jit_compiler,
                MODE_JIT,
                SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
                FUNCTION_TO_OPERAND(put_hash_table),
                3,
                REGISTER_TO_OPERAND(struct_addr_reg),
                IMMEDIATE_TO_OPERAND((uint64_t) current_field_name),
                REGISTER_TO_OPERAND(pop_result.operand.as.reg)
        );
        pop_register_allocator(&jit_compiler->register_allocator);
    }

    //Load struct pointer from struct's field pointer
    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    register_t struct_lox_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_lox_addr_reg),
             REGISTER_TO_OPERAND(struct_addr_reg));

    cast_lox_object_to_ptr(jit_compiler, struct_lox_addr_reg);

    push_heap_allocated_register_jit_stack(&jit_compiler->jit_stack, struct_lox_addr_reg);

    //pop struct_lox_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_INITIALIZE_STRUCT_LENGTH);
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
    runtime_panic("Invalid OP_DEFINE_GLOBAL bytecode to jit compile");
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

    struct pop_stack_operand_result new_global_value = pop_stack_operand_jit_stack(jit_compiler);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(put_hash_table),
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) global_name),
            new_global_value.operand
    );

    push_operand_jit_stack(&jit_compiler->jit_stack, new_global_value.operand);

    record_compiled_bytecode(jit_compiler, PICK_FIRST_NOT_ZERO(new_global_value.instruction_index, instruction_index), OP_SET_GLOBAL_LENGTH);
}

static void get_global(struct jit_compiler * jit_compiler) {
    struct string_object * name = AS_STRING_OBJECT(READ_CONSTANT(jit_compiler));
    struct package * current_package = peek_stack_list(&jit_compiler->package_stack);

    register_t global_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(global_value_reg),
            LOX_ESP_REG_OPERAND);

    //The value_node will be allocated rigth after RSP
    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_hash_table),
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) name),
            REGISTER_TO_OPERAND(global_value_reg)
    );

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(global_value_reg),
             DISPLACEMENT_TO_OPERAND(global_value_reg, 0));

    push_register_jit_stack(&jit_compiler->jit_stack,global_value_reg);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_GLOBAL_LENGTH);
}

static void set_local(struct jit_compiler * jit_compiler) {
    uint8_t slot = READ_BYTECODE(jit_compiler);
    size_t offset_local_from_rbp = slot * sizeof(lox_value_t);

    struct pop_stack_operand_result new_value_local = pop_stack_operand_jit_stack_as_register(jit_compiler);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(LOX_EBP_REG, offset_local_from_rbp),
             new_value_local.operand);

    push_register_jit_stack(&jit_compiler->jit_stack, new_value_local.operand.as.reg);

    record_compiled_bytecode(jit_compiler,
                             PICK_FIRST_NOT_ZERO(new_value_local.instruction_index, instruction_index),
                             OP_SET_LOCAL_LENGTH);
}

static void get_local(struct jit_compiler * jit_compiler) {
    uint8_t slot = READ_BYTECODE(jit_compiler);

    size_t offset_local_from_rbp = slot * sizeof(lox_value_t);
    push_displacement_jit_stack(&jit_compiler->jit_stack, LOX_EBP_REG, offset_local_from_rbp);

    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_GET_LOCAL_LENGTH);
}

static void print(struct jit_compiler * jit_compiler) {
    struct pop_stack_operand_result to_print_operand = pop_stack_operand_jit_stack(jit_compiler);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(print_lox_value),
            1,
            to_print_operand.operand);

    if(to_print_operand.operand.type == REGISTER_OPERAND){
        pop_register_allocator(&jit_compiler->register_allocator);
    }

    record_multiple_compiled_bytecode(jit_compiler, OP_PRINT_LENGTH, 2, to_print_operand.instruction_index, instruction_index);
}

static void jump_if_false(struct jit_compiler * jit_compiler, uint16_t jump_offset) {
    uint16_t instruction_index = call_safepoint(jit_compiler);

    struct pop_stack_operand_result boolean_value = pop_stack_operand_jit_stack_as_register(jit_compiler);

    register_t register_true_lox_value = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_true_lox_value),
             IMMEDIATE_TO_OPERAND(TRUE_VALUE));
    emit_cmp(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(boolean_value.operand.as.reg),
             REGISTER_TO_OPERAND(register_true_lox_value));

    uint16_t jmp_index = emit_near_jne(&jit_compiler->native_compiled_code, 0);

    //deallocate register_true_lox_value
    pop_register_allocator(&jit_compiler->register_allocator);
    push_register_jit_stack(&jit_compiler->jit_stack, boolean_value.operand.as.reg);

    record_pending_jump_to_patch(jit_compiler, jmp_index, jump_offset, 2); //JNE takes two bytes as opcode
    record_compiled_bytecode(jit_compiler, instruction_index, OP_JUMP_IF_FALSE_LENGTH);
}

static void jump(struct jit_compiler * jit_compiler, uint16_t offset) {
    uint16_t jmp_index = emit_near_jmp(&jit_compiler->native_compiled_code, 0); //We don't know the offset of where to jump

    record_pending_jump_to_patch(jit_compiler, jmp_index, offset, 1); //jmp takes only 1 byte as opcode
    record_compiled_bytecode(jit_compiler, jmp_index, OP_JUMP_LENGTH);
}

static void loop(struct jit_compiler * jit_compiler, uint16_t bytecode_backward_jump) {
    uint16_t bytecode_index_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_backward_jump;
    uint16_t native_index_to_jump = find_native_index_by_compiled_bytecode(jit_compiler, bytecode_index_to_jump);
    uint16_t current_native_index = jit_compiler->native_compiled_code.in_use;

    // +5 because of instruction size
    uint16_t jmp_offset = (current_native_index - native_index_to_jump) + 5;

    //Loop bytecode instruction always jumps backward
    uint16_t jump_index = emit_near_jmp(&jit_compiler->native_compiled_code, -((int) jmp_offset));

    record_compiled_bytecode(jit_compiler, jump_index, OP_LOOP_LENGTH);
}

static void pop(struct jit_compiler * jit_compiler) {
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_jit_stack(&jit_compiler->jit_stack);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_POP_LENGTH);
}

static void constant(struct jit_compiler * jit_compiler) {
    push_immediate_jit_stack(&jit_compiler->jit_stack, READ_CONSTANT(jit_compiler));
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_CONSTANT_LENGTH);
}

static void lox_value(struct jit_compiler * jit_compiler, lox_value_t value, int bytecode_instruction_length) {
    push_immediate_jit_stack(&jit_compiler->jit_stack, value);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, bytecode_instruction_length);
}

static void number_const(struct jit_compiler * jit_compiler, int value, int instruction_length) {
    push_immediate_jit_stack(&jit_compiler->jit_stack, value);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, instruction_length);
}

static void record_pending_jump_to_patch(
        struct jit_compiler * jit_compiler,
        uint16_t jump_instruction_index, //Index to native code where jump instruction starts
        uint16_t bytecode_offset, //Bytecode offset to jump
        uint16_t x64_jump_instruction_body_length //Jump native instruction length (without counting the native jump offset)
) {
    uint16_t bytecode_instruction_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) + bytecode_offset;

    //Near jump instruction: (opcode 2 byte) + (offset 4 bytes)
    add_pending_jump_to_resolve(
            &jit_compiler->pending_jumps_to_resolve,
            bytecode_instruction_to_jump,
            (void *) (jump_instruction_index + x64_jump_instruction_body_length)
    );
}

static struct jit_compiler init_jit_compiler(struct function_object * function) {
    struct jit_compiler compiler;

    compiler.current_mode = MODE_JIT;
    compiler.compiled_bytecode_to_native_by_index = malloc(sizeof(uint16_t) * function->chunk->in_use);
    memset(compiler.compiled_bytecode_to_native_by_index, 0, sizeof(uint16_t) * function->chunk->in_use);

    compiler.last_stack_slot_allocated = function->n_arguments > 0 ? function->n_arguments : -1;
    compiler.function_to_compile = function;
    compiler.pc = function->chunk->code;

    init_pending_jumps_to_resolve(&compiler.pending_jumps_to_resolve, function->chunk->in_use);
    init_register_allocator(&compiler.register_allocator);
    init_u8_arraylist(&compiler.native_compiled_code);
    init_stack_list(&compiler.package_stack);
    init_jit_stack(&compiler.jit_stack);

    return compiler;
}

static void record_multiple_compiled_bytecode(struct jit_compiler * jit_compiler,
        int bytecode_instruction_length,
        int n_instrucion_indexes, ...) {
    int instrucion_indexes[n_instrucion_indexes];
    VARARGS_TO_ARRAY(int, instrucion_indexes, n_instrucion_indexes);

    for(int i = 0; i < n_instrucion_indexes; i++){
        if(instrucion_indexes[i] != 0){
            record_compiled_bytecode(jit_compiler, instrucion_indexes[i], bytecode_instruction_length);
            break;
        }
    }
}

static void record_compiled_bytecode(struct jit_compiler * jit_compiler, uint16_t native_compiled_index, int bytecode_instruction_length) {
    uint16_t bytecode_compiled_index = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_instruction_length;

    if(jit_compiler->compiled_bytecode_to_native_by_index[bytecode_compiled_index] == 0){
        jit_compiler->compiled_bytecode_to_native_by_index[bytecode_compiled_index] = native_compiled_index;
        check_pending_jumps_to_patch(jit_compiler, bytecode_instruction_length);
    }
}

//Assembly implementation of AS_OBJECT defined in types.h
//AS_OBJECT(value_node) ((struct object *) (uintptr_t)((value_node) & ~(FLOAT_SIGN_BIT | FLOAT_QNAN)))
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
//TO_LOX_VALUE_OBJECT(value_node) (FLOAT_SIGN_BIT | FLOAT_QNAN | (lox_value_t) value_node)
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

    struct pending_jump_to_resolve pending_jump_to_patch = get_pending_jump_to_resolve(
            &jit_compiler->pending_jumps_to_resolve,
            current_bytecode_index);

    for(int i = 0; i < pending_jump_to_patch.in_use; i++){
        uint16_t compiled_native_jmp_offset_index = (uint16_t) pending_jump_to_patch.pending_resolve_data[i];

        if (compiled_native_jmp_offset_index != 0) {
            //pending_resolve_data points to native jmp offset part of the instruction
            //-4 to substract the jmp offset since it takes 4 bytes
            uint16_t native_jmp_offset = current_compiled_index - (compiled_native_jmp_offset_index + 4);
            uint16_t * compiled_native_jmp_offset_index_ptr = (uint16_t *) (jit_compiler->native_compiled_code.values + compiled_native_jmp_offset_index);

            *compiled_native_jmp_offset_index_ptr = native_jmp_offset;
        }
    }
}

//As the vm stack might have some profile_data, we need to update vm esp, so that if any thread is garbage collecting,
//it can read the most up-to-date stack without discarding profile_data
//This is only used here since the safepoint calls are done in places where the stack is emtpy
//Note that the stack only contains values when an expression is being executed.
static uint16_t call_safepoint(struct jit_compiler * jit_compiler) {
    return call_external_c_function(
            jit_compiler,
            MODE_VM_GC,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(check_gc_on_safe_point_alg),
            0);
}

//Se x64_stack.h
//Same as vm.c push_stack_vm
static uint16_t emit_lox_push(struct jit_compiler * jit_compiler, register_t reg) {
    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(LOX_ESP_REG, 0),
             REGISTER_TO_OPERAND(reg));

    emit_increase_lox_stack(jit_compiler, 1);

    return instruction_index;
}

//Same as vm.c pop_stack_vm
static uint16_t emit_lox_pop(struct jit_compiler * jit_compiler, register_t reg) {
    uint16_t instruction_index = emit_decrease_lox_stack(jit_compiler, 1);

    emit_mov(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(reg),
            DISPLACEMENT_TO_OPERAND(LOX_ESP_REG, 0));

    return instruction_index;
}

static uint16_t emit_increase_lox_stack(struct jit_compiler * jit_compiler, int n_locals) {
    return emit_add(&jit_compiler->native_compiled_code,
            LOX_ESP_REG_OPERAND,
            IMMEDIATE_TO_OPERAND(sizeof(lox_value_t) * n_locals));
}

static uint16_t emit_decrease_lox_stack(struct jit_compiler * jit_compiler, int n_locals) {
    return emit_sub(&jit_compiler->native_compiled_code,
            LOX_ESP_REG_OPERAND,
            IMMEDIATE_TO_OPERAND(sizeof(lox_value_t) * n_locals));
}

static uint16_t get_compiled_native_index_by_bytecode_index(struct jit_compiler * jit_compiler, uint16_t current_bytecode_index) {
    uint16_t * current = jit_compiler->compiled_bytecode_to_native_by_index + current_bytecode_index;

    if(*current == NATIVE_INDEX_IN_NEXT_SLOT) {
        return jit_compiler->native_compiled_code.in_use;
    } else {
        return *current;
    }
}

static void free_jit_compiler(struct jit_compiler * jit_compiler) {
    free_pending_jumps_to_resolve(&jit_compiler->pending_jumps_to_resolve);
    free(jit_compiler->compiled_bytecode_to_native_by_index);
    free_stack_list(&jit_compiler->package_stack);
}

static struct pop_stack_operand_result pop_stack_operand_jit_stack_as_register(struct jit_compiler * jit_compiler) {
    struct pop_stack_operand_result item_from_stack = pop_stack_operand_jit_stack(jit_compiler);

    if(item_from_stack.operand.type == IMMEDIATE_OPERAND){
        //Is immediate value_node
        register_t item_reg = push_register_allocator(&jit_compiler->register_allocator);

        uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
                REGISTER_TO_OPERAND(item_reg),
                IMMEDIATE_TO_OPERAND(item_from_stack.operand.as.immediate));

        return (struct pop_stack_operand_result) {REGISTER_TO_OPERAND(item_reg), instruction_index};
    } else {
        return item_from_stack;
    }
}

//Onlu returns IMMEDIATE_OPERAND or REGISTER_OPERAND
static struct pop_stack_operand_result pop_stack_operand_jit_stack(struct jit_compiler * jit_compiler) {
    struct jit_stack_item item = pop_jit_stack(&jit_compiler->jit_stack);

    switch (item.type) {
        case IMMEDIATE_JIT_STACK_ITEM: {
            return (struct pop_stack_operand_result) {IMMEDIATE_TO_OPERAND(item.as.immediate), 0};
        }
        case REGISTER_JIT_STACK_ITEM: {
            return (struct pop_stack_operand_result) {REGISTER_TO_OPERAND(item.as.reg), 0};
        }
        case DISPLACEMENT_JIT_STACK_ITEM: {
            register_t operand_reg = push_register_allocator(&jit_compiler->register_allocator);
            uint8_t index = emit_mov(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(operand_reg), item.as.displacement);
            return (struct pop_stack_operand_result) {REGISTER_TO_OPERAND(operand_reg), index};
        }
        case NATIVE_STACK_JIT_STACK_ITEM: {
            register_t operand_reg = push_register_allocator(&jit_compiler->register_allocator);
            uint8_t index = emit_pop(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(operand_reg));
            return (struct pop_stack_operand_result) {REGISTER_TO_OPERAND(operand_reg), index};
        }
    }

    runtime_panic("Illegal state in x64 JIT bytecode_compiler, in pop_stack_operand_jit_stack. Invalid stack item type %i", item.type);
}

static void single_operation(
        struct jit_compiler * jit_compiler,
        int instruction_length,
        bytecode_t instruction) {
    struct pop_stack_operand_result a = pop_stack_operand_jit_stack(jit_compiler);
    struct single_operation binary_operations_instruction = single_operations[instruction];

    if(a.operand.type == IMMEDIATE_OPERAND){
        uint64_t result = binary_operations_instruction.imm_operation(jit_compiler, a.operand);
        push_immediate_jit_stack(&jit_compiler->jit_stack, result);
        record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, instruction_length);
        return;
    } else if(a.operand.type == REGISTER_OPERAND){
        binary_operations_instruction.reg_operation(jit_compiler, a.operand);
        record_compiled_bytecode(jit_compiler, a.instruction_index, instruction_length);

        if(does_single_bytecode_instruction(*jit_compiler->pc)){
            push_register_jit_stack(&jit_compiler->jit_stack, a.operand.as.reg);
        } else {
            pop_register_allocator(&jit_compiler->register_allocator);
            emit_push(&jit_compiler->native_compiled_code, a.operand); //pop a
            push_native_stack_jit_stack(&jit_compiler->jit_stack);
        }
    }
}

static void binary_operation(
        struct jit_compiler * jit_compiler,
        int instruction_length,
        bytecode_t instruction
) {
    struct pop_stack_operand_result b = pop_stack_operand_jit_stack(jit_compiler);
    struct pop_stack_operand_result a = pop_stack_operand_jit_stack(jit_compiler);
    struct operand result_operand;

    struct binary_operation binary_operations_instruction = binary_operations[instruction];

    if(a.operand.type == IMMEDIATE_OPERAND && b.operand.type == IMMEDIATE_OPERAND) {
        uint64_t result = binary_operations_instruction.imm_imm_operation(jit_compiler, a.operand, b.operand);
        push_immediate_jit_stack(&jit_compiler->jit_stack, result);
        record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, instruction_length);
        return;

    } else if(a.operand.type == REGISTER_OPERAND && b.operand.type == REGISTER_OPERAND) {
        //Pop a & b
        pop_register_allocator(&jit_compiler->register_allocator);
        pop_register_allocator(&jit_compiler->register_allocator);

        binary_operations_instruction.reg_reg_operation(jit_compiler, a.operand, b.operand);

        //Push result, has larges register number
        register_t result_reg = push_register_allocator(&jit_compiler->register_allocator);
        emit_mov(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(result_reg),
                 a.operand);

        record_compiled_bytecode(jit_compiler, b.instruction_index, instruction_length);
        result_operand = REGISTER_TO_OPERAND(result_reg);

    } else { //1 register, 1 operand
        struct pop_stack_operand_result operand_reg = a.operand.type == REGISTER_OPERAND ? a : b;
        struct pop_stack_operand_result operand_imm = a.operand.type == REGISTER_OPERAND ? b : a;

        if(!IS_QWORD_SIZE(operand_imm.operand.as.immediate)){
            binary_operations_instruction.reg_imm_operation(jit_compiler, operand_reg.operand, operand_imm.operand);
            record_compiled_bytecode(jit_compiler, operand_reg.instruction_index, instruction_length);

        } else {
            register_t temp = push_register_allocator(&jit_compiler->register_allocator);

            emit_mov(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(temp), operand_imm.operand);

            binary_operations_instruction.reg_reg_operation(jit_compiler, operand_reg.operand,
                                                            REGISTER_TO_OPERAND(temp));

            pop_register_allocator(&jit_compiler->register_allocator); //Pop temp

            record_compiled_bytecode(jit_compiler, operand_reg.instruction_index, instruction_length);
        }

        result_operand = operand_reg.operand;
    }

    if(does_single_bytecode_instruction(*jit_compiler->pc)){
        push_register_jit_stack(&jit_compiler->jit_stack, result_operand.as.reg);
    } else {
        pop_register_allocator(&jit_compiler->register_allocator);
        emit_push(&jit_compiler->native_compiled_code, result_operand);
        push_native_stack_jit_stack(&jit_compiler->jit_stack);
    }
}

static uint16_t find_native_index_by_compiled_bytecode(struct jit_compiler * jit_compiler, uint16_t bytecode_index) {
    uint16_t * current_native_index = &jit_compiler->compiled_bytecode_to_native_by_index[bytecode_index];

    while(*current_native_index == NATIVE_INDEX_IN_NEXT_SLOT || *current_native_index == 0) {
        current_native_index++;
    }

    return * current_native_index;
}

static bool does_single_bytecode_instruction(bytecode_t opcode) {
    switch (opcode) {
        case OP_POP:
        case OP_RETURN:
        case OP_NEGATE:
        case OP_PRINT:
        case OP_DEFINE_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_SET_LOCAL:
        case OP_NOT:
        case OP_JUMP_IF_FALSE:
        case OP_GET_STRUCT_FIELD:
        case OP_ENTER_PACKAGE:
        case OP_GET_ARRAY_ELEMENT:
            return true;
        default:
            return false;
    }
}

static void wrapper_write_array_element_barrier (struct object * array_object, lox_value_t value) {
    if (IS_OBJECT(value)) {
        get_barriers_gc_alg().write_array_element((struct array_object *) array_object, AS_OBJECT(value));
    }
}

static void wrapper_write_struct_field_barrier (struct object * struct_object, lox_value_t value) {
    if (IS_OBJECT(value)) {
        get_barriers_gc_alg().write_struct_field((struct struct_instance_object *) struct_object, AS_OBJECT(value));
    }
}