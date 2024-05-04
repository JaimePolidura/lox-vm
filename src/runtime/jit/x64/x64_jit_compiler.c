#include "x64_jit_compiler.h"

extern __thread struct vm_thread * self_thread;

extern bool get_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t *value);
extern bool put_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);
extern struct call_frame * get_current_frame_vm_thread(struct vm_thread *);
extern bool set_element_array(struct array_object * array, int index, lox_value_t value);
extern struct struct_instance_object * alloc_struct_instance_object(struct struct_definition_object *);
extern void add_object_to_heap_gc_alg(struct object * object);
extern struct array_object * alloc_array_object(int size);
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
extern void switch_jit_to_native_mode(struct jit_compiler *);
extern struct binary_operation binary_operations[];
extern struct single_operation single_operations[];

void switch_native_to_jit_mode(struct jit_compiler *);

//Used by jit_compiler::compiled_bytecode_to_native_by_index Some operations are not compiled to native code, but some jumps bytecode offset
//will be pointing to those operations. If in a slot is -1, the native offset will be in the next slot
#define NATIVE_INDEX_IN_NEXT_SLOT 0xFFFF

//extern __thread struct vm_thread * self_thread;

#define READ_BYTECODE(jit_compiler) (*(jit_compiler)->pc++)
#define READ_U16(jit_compiler) \
    ((jit_compiler)->pc += 2, (uint16_t)(((jit_compiler)->pc[-2] << 8) | (jit_compiler)->pc[-1]))
#define READ_CONSTANT(jit_compiler) (jit_compiler->function_to_compile->chunk.constants.values[READ_BYTECODE(jit_compiler)])
#define CURRENT_BYTECODE_INDEX(jit_compiler) (jit_compiler->pc - jit_compiler->function_to_compile->chunk.code)

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
static void exti_monitor_jit(struct jit_compiler *);
static void return_jit(struct jit_compiler *, bool * finish_compilation_flag);
static void call(struct jit_compiler *);
static void eof(struct jit_compiler *, bool * finish_compilation_flag);

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
static void exit_monitor_jit(struct jit_compiler *);
static uint16_t call_safepoint(struct jit_compiler *);
static uint16_t call_add_object_to_heap(struct jit_compiler *, register_t);
static uint16_t emit_lox_push(struct jit_compiler *, register_t);
static uint16_t emit_lox_pop(struct jit_compiler *, register_t);
static uint16_t emit_increase_lox_stack(struct jit_compiler *, int);
static uint16_t emit_decrease_lox_stack(struct jit_compiler *jit_compiler, int n_locals);
static void emit_native_call(struct jit_compiler *, register_t function_object_addr_reg, int n_args);
static struct pop_stack_operand_result pop_stack_operand_jit_stack(struct jit_compiler *);
static void record_multiple_compiled_bytecode(struct jit_compiler * jit_compiler,
        int bytecode_instruction_length,int n_instrucion_indexes, ...);
static void binary_operation(struct jit_compiler * jit_compiler, int instruction_length, op_code instruction);
static void single_operation(struct jit_compiler * jit_compiler, int instruction_length, op_code instruction);

static void call(struct jit_compiler * jit_compiler) {
    int n_args = READ_BYTECODE(jit_compiler);
    bool is_parallel = READ_BYTECODE(jit_compiler);
    register_t function_register = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = cast_ptr_to_lox_object(jit_compiler, function_register);

    //Pop function_register
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_cmp(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_register),
             IMMEDIATE_TO_OPERAND(OBJ_NATIVE_FUNCTION));

    emit_native_call(jit_compiler, function_register, n_args);

    call_safepoint(jit_compiler);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_CALL_LENGTH);
}

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
            case OP_ENTER_MONITOR: enter_monitor_jit(&jit_compiler); break;
            case OP_EXIT_MONITOR: exit_monitor_jit(&jit_compiler); break;
            case OP_RETURN: return_jit(&jit_compiler, &finish_compilation_flag); break;
            case OP_CALL: call(&jit_compiler); break;
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

static void eof(struct jit_compiler * jit_compiler, bool * finish_compilation_flag) {
    *finish_compilation_flag = true;

    record_compiled_bytecode(jit_compiler,
                             jit_compiler->native_compiled_code.in_use,
                             OP_EOF_LENGTH);
}

static void emit_native_call(struct jit_compiler * jit_compiler, register_t function_object_addr_reg, int n_args) {
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(function_object_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct native_function_object, native_fn)));

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
            MODE_JIT,
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
    struct pop_stack_operand_result pop_stack_operand_result = pop_stack_operand_jit_stack(jit_compiler);
    if(pop_stack_operand_result.operand.type == IMMEDIATE_OPERAND){
        register_t returned_value_reg = push_register_allocator(&jit_compiler->register_allocator);
        emit_mov(&jit_compiler->native_compiled_code,
                 REGISTER_TO_OPERAND(returned_value_reg),
                 IMMEDIATE_TO_OPERAND(pop_stack_operand_result.operand.as.immediate));
        emit_lox_push(jit_compiler, returned_value_reg);
        pop_register_allocator(&jit_compiler->register_allocator);
    } else {
        emit_lox_push(jit_compiler, pop_stack_operand_result.operand.as.reg);
        pop_register_allocator(&jit_compiler->register_allocator);
    }

    //We update self_thread with the new esp value
    emit_mov(&jit_compiler->native_compiled_code,
             DISPLACEMENT_TO_OPERAND(SELF_THREAD_ADDR_REG, offsetof(struct vm_thread, esp)),
             LOX_ESP_REG_OPERAND);

    record_compiled_bytecode(jit_compiler, instruction_index, OP_RETURN_LENGTH);

    *finish_compilation_flag = true;
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

static void enter_monitor_jit(struct jit_compiler * jit_compiler) {
    monitor_number_t monitor_number_to_enter = READ_BYTECODE(jit_compiler);
    struct monitor * monitor_to_enter = &jit_compiler->function_to_compile->monitors[monitor_number_to_enter];

    //set_self_thread_waiting calls safepoint that run in vm mode
    //(another thread might be doing gc, so it will need to read the most up-to-date lox stack)
    uint16_t instruction_index = call_external_c_function(
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

static void set_array_element(struct jit_compiler * jit_compiler) {
    uint16_t array_index = READ_U16(jit_compiler);

    register_t element_addr_reg = peek_at_register_allocator(&jit_compiler->register_allocator, 0);
    register_t new_element = peek_at_register_allocator(&jit_compiler->register_allocator, 1);

    uint16_t instruction_index = cast_lox_object_to_ptr(jit_compiler, element_addr_reg);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(element_addr_reg),
             IMMEDIATE_TO_OPERAND(
                     offsetof(struct array_object, values) +
                     offsetof(struct lox_arraylist, values)));

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
    uint16_t instruction_index = 0;

    //Load elements in order from registers to lox stack
    for(int i = 0; i < n_elements; i++){
        register_t reg = peek_at_register_allocator(&jit_compiler->register_allocator, i);
        uint16_t current_instruction_index = emit_lox_push(jit_compiler, reg);

        if(i == 0)
            instruction_index = current_instruction_index;
    }
    pop_at_register_allocator(&jit_compiler->register_allocator, n_elements);

    //Allocate array object & add to heap list
    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(alloc_array_object),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) n_elements));

    register_t array_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(array_addr_reg),
             RAX_REGISTER_OPERAND);
    call_add_object_to_heap(jit_compiler, array_addr_reg);

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
    register_t array_value_reg = push_register_allocator(&jit_compiler->register_allocator);
    for(int i = 0; i < n_elements; i++) {
        emit_lox_pop(jit_compiler, array_value_reg);

        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(array_values_addr_reg, i * sizeof(lox_value_t)),
                 REGISTER_TO_OPERAND(array_value_reg));
    }

    cast_ptr_to_lox_object(jit_compiler, array_addr_reg);

    //Pop array_value_reg, array_addr_reg & array_values_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(push_register_allocator(&jit_compiler->register_allocator)),
             REGISTER_TO_OPERAND(array_addr_reg));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_INITIALIZE_ARRAY_LENGTH);
}

static void set_struct_field(struct jit_compiler * jit_compiler) {
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    register_t new_value_reg = peek_at_register_allocator(&jit_compiler->register_allocator, 0);
    register_t struct_instance_addr_reg = peek_at_register_allocator(&jit_compiler->register_allocator, 1);

    uint16_t first_instruction_index = cast_lox_object_to_ptr(jit_compiler, struct_instance_addr_reg);

    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_instance_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(put_hash_table),
            3,
            REGISTER_TO_OPERAND(struct_instance_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            REGISTER_TO_OPERAND(new_value_reg));

    //Deallocate new_value_reg & struct_instance_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    record_compiled_bytecode(jit_compiler, first_instruction_index, OP_SET_STRUCT_FIELD_LENGTH);
}

static void get_struct_field(struct jit_compiler * jit_compiler) {
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));

    register_t struct_instance_addr_reg = peek_register_allocator(&jit_compiler->register_allocator);
    uint16_t instruction_index = cast_lox_object_to_ptr(jit_compiler, struct_instance_addr_reg);
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_instance_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));

    register_t field_value_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(field_value_reg),
            LOX_ESP_REG_OPERAND);

    //The value will be allocated rigth after RSP. RSP always points to the first non-used slot of the stack
    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(get_hash_table),
            3,
            REGISTER_TO_OPERAND(struct_instance_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            REGISTER_TO_OPERAND(field_value_reg));

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(field_value_reg),
             DISPLACEMENT_TO_OPERAND(field_value_reg, 0));

    //pop struct_instance_addr_reg & struct_instance_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(push_register_allocator(&jit_compiler->register_allocator)),
             REGISTER_TO_OPERAND(field_value_reg));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_STRUCT_FIELD_LENGTH);
}

static void initialize_struct(struct jit_compiler * jit_compiler) {
    struct struct_definition_object * struct_definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    int n_fields = struct_definition->n_fields;
    uint16_t instruction_index = 0;

    //Load struct fields into lox stack
    for(int i = 0; i < n_fields; i++){
        register_t reg = peek_at_register_allocator(&jit_compiler->register_allocator, i);
        uint16_t current_instrution_index = emit_lox_push(jit_compiler, reg);

        if(i == 0)
            instruction_index = current_instrution_index;
    }
    pop_at_register_allocator(&jit_compiler->register_allocator, n_fields);

    //Alloc struct_instance_objet, add to heap & load struct address into struct_addr_reg
    call_external_c_function(
            jit_compiler,
            MODE_JIT,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(alloc_struct_instance_object),
            1,
            IMMEDIATE_TO_OPERAND((uint64_t) struct_definition)
    );
    register_t struct_addr_reg = push_register_allocator(&jit_compiler->register_allocator);
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             RAX_REGISTER_OPERAND);
    call_add_object_to_heap(jit_compiler, struct_addr_reg);
    emit_add(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    //Add struct fields to struct
    register_t field_value_register = push_register_allocator(&jit_compiler->register_allocator);
    for(int i = 0; i < n_fields; i++){
        emit_lox_pop(jit_compiler, field_value_register);
        call_external_c_function(
                jit_compiler,
                MODE_JIT,
                SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
                FUNCTION_TO_OPERAND(put_hash_table),
                3,
                REGISTER_TO_OPERAND(struct_addr_reg),
                IMMEDIATE_TO_OPERAND((uint64_t) struct_definition->field_names[i]),
                REGISTER_TO_OPERAND(field_value_register)
        );
    }

    //Cast the lox object to lox pointer
    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(struct_addr_reg),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));
    cast_lox_object_to_ptr(jit_compiler, struct_addr_reg);

    //Pop field_value_register & struct_addr_reg
    pop_register_allocator(&jit_compiler->register_allocator);
    pop_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(push_register_allocator(&jit_compiler->register_allocator)),
             REGISTER_TO_OPERAND(struct_addr_reg));

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
    struct string_object * global_name = AS_STRING_OBJECT(READ_CONSTANT(jit_compiler));
    struct package * current_package = peek_stack_list(&jit_compiler->package_stack);

    register_t new_global_value = peek_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            jit_compiler,
            MODE_JIT,
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
            MODE_JIT,
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

    register_t global_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
            REGISTER_TO_OPERAND(global_value_reg),
            LOX_ESP_REG_OPERAND);

    //The value will be allocated rigth after RSP
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

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_GLOBAL_LENGTH);
}

static void set_local(struct jit_compiler * jit_compiler) {
    uint8_t slot = READ_BYTECODE(jit_compiler);
    size_t offset_local_from_rbp = slot * sizeof(lox_value_t);

    struct pop_stack_operand_result to_print_operand = pop_stack_operand_jit_stack(jit_compiler);
    uint16_t instruction_index = 0;

    if(to_print_operand.operand.type == IMMEDIATE_OPERAND){
        register_t register_value = push_register_allocator(&jit_compiler->register_allocator);
        instruction_index = emit_mov(&jit_compiler->native_compiled_code,
                REGISTER_TO_OPERAND(register_value),
                IMMEDIATE_TO_OPERAND(to_print_operand.operand.as.immediate));
        emit_mov(&jit_compiler->native_compiled_code,
                 DISPLACEMENT_TO_OPERAND(LOX_EBP_REG, offset_local_from_rbp),
                 REGISTER_TO_OPERAND(register_value));
        //pop register_value
        pop_register_allocator(&jit_compiler->register_allocator);

    } else { //Register operand
        instruction_index = emit_mov(&jit_compiler->native_compiled_code,
                DISPLACEMENT_TO_OPERAND(LOX_EBP_REG, offset_local_from_rbp),
                to_print_operand.operand);

        pop_register_allocator(&jit_compiler->register_allocator);
    }

    record_compiled_bytecode(jit_compiler,
            PICK_NOT_ZERO(to_print_operand.instruction_index, instruction_index),
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
    uint16_t jmp_index = emit_near_jmp(&jit_compiler->native_compiled_code, 0); //We don't know the offset of where to jump

    record_pending_jump_to_patch(jit_compiler, jmp_index, offset, 1); //jmp takes only 1 byte as opcode
    record_compiled_bytecode(jit_compiler, jmp_index, OP_JUMP_LENGTH);
}

static void pop(struct jit_compiler * jit_compiler) {
    pop_jit_stack(&jit_compiler->jit_stack);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_POP_LENGTH);
}

static void loop(struct jit_compiler * jit_compiler, uint16_t bytecode_backward_jump) {
    uint16_t bytecode_index_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_backward_jump;
    uint16_t native_index_to_jump = jit_compiler->compiled_bytecode_to_native_by_index[bytecode_index_to_jump];
    uint16_t current_native_index = jit_compiler->native_compiled_code.in_use;

    // +5 because of instruction size
    uint16_t jmp_offset = (current_native_index - native_index_to_jump) + 5;

    //Loop bytecode instruction always jumps backward
    uint16_t jump_index = emit_near_jmp(&jit_compiler->native_compiled_code, -((int) jmp_offset));

    record_compiled_bytecode(jit_compiler, jump_index, OP_LOOP_LENGTH);
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

static void record_pending_jump_to_patch(struct jit_compiler * jit_compiler,
        uint16_t jump_instruction_index, //Index to native code where jump instruction starts
        uint16_t bytecode_offset, //Bytecode offset to jump
        uint16_t x64_jump_instruction_body_length //Jump native instruction length (without counting the native jump offset)
) {
    uint16_t bytecode_instruction_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) + bytecode_offset;

    if(jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump] == NULL) {
        jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump] = alloc_pending_path_jump();
    }

    //Near jump instruction: (opcode 2 byte) + (offset 4 bytes)
    add_pending_path_jump(
            jit_compiler->pending_jumps_to_patch[bytecode_instruction_to_jump],
            jump_instruction_index + x64_jump_instruction_body_length);
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
    return call_external_c_function(
            jit_compiler,
            MODE_VM,
            SWITCH_BACK_TO_PREV_MODE_AFTER_CALL,
            FUNCTION_TO_OPERAND(add_object_to_heap_gc_alg),
            1,
            REGISTER_TO_OPERAND(object_addr_reg));
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
    for(int i = 0; i < jit_compiler->function_to_compile->chunk.in_use; i++){
        if(jit_compiler->pending_jumps_to_patch[i] != NULL){
            free(jit_compiler->pending_jumps_to_patch[i]);
        }
    }
    free_stack_list(&jit_compiler->package_stack);
    free(jit_compiler->pending_jumps_to_patch);
    free(jit_compiler->compiled_bytecode_to_native_by_index);
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
            uint8_t index =emit_mov(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(operand_reg), item.as.displacement);
            return (struct pop_stack_operand_result) {REGISTER_TO_OPERAND(operand_reg), index};
        }
        case NATIVE_STACK_JIT_STACK_ITEM: {
            register_t operand_reg = push_register_allocator(&jit_compiler->register_allocator);
            uint8_t index =emit_pop(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(operand_reg));
            return (struct pop_stack_operand_result) {REGISTER_TO_OPERAND(operand_reg), index};
        }
    }

    runtime_panic("Illegal state in x64 JIT compiler, in pop_stack_operand_jit_stack. Invalid stack item type %i", item.type);
}

static void single_operation(
        struct jit_compiler * jit_compiler,
        int instruction_length,
        op_code instruction) {
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

        if(does_single_pop_vm_stack(*jit_compiler->pc)){
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
        op_code instruction
) {
    struct pop_stack_operand_result a = pop_stack_operand_jit_stack(jit_compiler);
    struct pop_stack_operand_result b = pop_stack_operand_jit_stack(jit_compiler);
    struct operand result_operand;

    struct binary_operation binary_operations_instruction = binary_operations[instruction];

    if(a.operand.type == IMMEDIATE_OPERAND && b.operand.type == IMMEDIATE_OPERAND) {
        uint64_t result = binary_operations_instruction.imm_imm_operation(jit_compiler, a.operand, b.operand);
        push_immediate_jit_stack(&jit_compiler->jit_stack, result);
        record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, instruction_length);
        return;

    } else if(a.operand.type == REGISTER_OPERAND && b.operand.type == REGISTER_OPERAND) {
        binary_operations_instruction.reg_reg_operation(jit_compiler, a.operand, b.operand);
        pop_register_allocator(&jit_compiler->register_allocator);
        record_compiled_bytecode(jit_compiler, a.instruction_index, instruction_length);
        result_operand = a.operand;

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

    if(does_single_pop_vm_stack(*jit_compiler->pc)){
        push_register_jit_stack(&jit_compiler->jit_stack, result_operand.as.reg);
    } else {
        pop_register_allocator(&jit_compiler->register_allocator);
        emit_push(&jit_compiler->native_compiled_code, result_operand);
        push_native_stack_jit_stack(&jit_compiler->jit_stack);
    }
}