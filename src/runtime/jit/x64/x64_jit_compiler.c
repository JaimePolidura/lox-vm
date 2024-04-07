#include "x64_jit_compiler.h"

extern bool get_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t *value);
extern bool put_hash_table(struct lox_hash_table * table, struct string_object * key, lox_value_t value);
extern struct struct_instance_object * alloc_struct_instance_object();
extern void print_lox_value(lox_value_t value);
extern void runtime_panic(char * format, ...);

//Used by jit_compiler::compiled_bytecode_to_native_by_index Some instructions are not compiled to native code, but some jumps bytecode offset
//will be pointing to those instructions. If in a slot is -1, the native offset will be in the next slot
#define NATIVE_INDEX_IN_NEXT_SLOT 0xFFFF

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
static void set_global(struct jit_compiler * jit_compiler);
static void define_global(struct jit_compiler * jit_compiler);
static void package_const(struct jit_compiler * jit_compiler);
static void initialize_struct(struct jit_compiler * jit_compiler);
static void get_struct_field(struct jit_compiler * jit_compiler);
static void set_struct_field(struct jit_compiler * jit_compiler);
static void nop(struct jit_compiler * jit_compiler);

static void record_pending_jump_to_patch(struct jit_compiler * jit_compiler, uint16_t jump_instruction_index,
        uint16_t bytecode_offset, uint16_t x64_jump_instruction_body_length);
static void record_compiled_bytecode(struct jit_compiler * jit_compiler, uint16_t native_compiled_index, int bytecode_instruction_length);
static uint16_t get_compiled_native_index_by_bytecode_index(struct jit_compiler * jit_compiler, uint16_t current_bytecode_index);
static void check_pending_jumps_to_patch(struct jit_compiler * jit_compiler, int bytecode_instruction_length);
static void cast_to_lox_boolean(struct jit_compiler * jit_compiler, register_t register_boolean_value);
static void set_al_with_cmp_result(struct jit_compiler * jit_compiler, op_code comparation_opcode);
static void number_const(struct jit_compiler * jit_compiler, int value, int instruction_length);
static void free_jit_compiler(struct jit_compiler * jit_compiler);
static uint16_t cast_lox_object_to_ptr(struct jit_compiler * jit_compiler, register_t lox_object_ptr);
static uint16_t cast_ptr_to_lox_object(struct jit_compiler * jit_compiler, register_t lox_object_ptr);

struct jit_compilation_result jit_compile(struct function_object * function) {
    struct jit_compiler jit_compiler = init_jit_compiler(function);
    bool finish_compilation_flag = false;

    prepare_x64_stack(&jit_compiler.native_compiled_code, function);

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
            &jit_compiler->native_compiled_code,
            (uint64_t) &put_hash_table,
            3,
            REGISTER_TO_OPERAND(struct_instance_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            REGISTER_TO_OPERAND(new_value_reg)
            );

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

    int return_value_disp = jit_compiler->last_stack_slot_allocated * sizeof(lox_value_t);

    emit_sub(&jit_compiler->native_compiled_code, RSP_OPERAND, IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));

    //The value will be allocated rigth after ESP
    call_external_c_function(
            &jit_compiler->native_compiled_code,
            (uint64_t) &get_hash_table,
            3,
            REGISTER_TO_OPERAND(struct_instance_addr_reg),
            IMMEDIATE_TO_OPERAND((uint64_t) field_name),
            DISPLACEMENT_TO_OPERAND(RBP, return_value_disp));

    register_t field_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(field_value_reg),
             DISPLACEMENT_TO_OPERAND(RBP, return_value_disp));

    emit_add(&jit_compiler->native_compiled_code, RSP_OPERAND, IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));

    record_compiled_bytecode(jit_compiler, first_instruction_index, OP_GET_STRUCT_FIELD_LENGTH);
}

static void initialize_struct(struct jit_compiler * jit_compiler) {
    struct struct_definition_object * struct_definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(jit_compiler));
    int n_fields = struct_definition->n_fields;

    uint16_t first_instruction_index = call_external_c_function(
            &jit_compiler->native_compiled_code,
            (uint64_t) &alloc_struct_instance_object,
            0
    );

    emit_add(&jit_compiler->native_compiled_code,
             RAX_OPERAND,
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields))
    );

    //RAX will get overrided by call_external_c_function because it is where the return address will be stored
    emit_push(&jit_compiler->native_compiled_code, RAX_OPERAND);

    //structs will always have to contain atleast one argument, so this for loop will get executed -> RAX will get popped
    for(int i = 0; i < n_fields; i++) {
        struct string_object * field_name = struct_definition->field_names[struct_definition->n_fields - i - 1];
        register_t struct_field_value_reg = pop_register_allocator(&jit_compiler->register_allocator);

        call_external_c_function(
                &jit_compiler->native_compiled_code,
                (uint64_t) &put_hash_table,
                3,
                RAX_OPERAND,
                IMMEDIATE_TO_OPERAND((uint64_t) field_name),
                REGISTER_TO_OPERAND(struct_field_value_reg)
        );

        //So that we can reuse it in the next call_external_c_function
        emit_pop(&jit_compiler->native_compiled_code, RAX_OPERAND);
    }

    emit_sub(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(RAX),
             IMMEDIATE_TO_OPERAND(offsetof(struct struct_instance_object, fields)));
    
    //Store struct instance address
    register_t register_to_store_address = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_to_store_address),
             RAX_OPERAND);

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
            &jit_compiler->native_compiled_code,
            (uint64_t) &put_hash_table,
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) global_name),
            REGISTER_TO_OPERAND(new_global_value)
    );

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
            &jit_compiler->native_compiled_code,
            (uint64_t) &put_hash_table,
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
    int return_value_dips = jit_compiler->last_stack_slot_allocated * sizeof(lox_value_t);

    uint16_t instruction_index = emit_sub(&jit_compiler->native_compiled_code,
            RSP_OPERAND,
            IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));

    //The value will be allocated rigth after ESP
    call_external_c_function(
            &jit_compiler->native_compiled_code,
            (uint64_t) &get_hash_table,
            3,
            IMMEDIATE_TO_OPERAND((uint64_t) &current_package->global_variables),
            IMMEDIATE_TO_OPERAND((uint64_t) name),
            DISPLACEMENT_TO_OPERAND(RBP, return_value_dips)
    );

    register_t global_value_reg = push_register_allocator(&jit_compiler->register_allocator);

    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(global_value_reg),
             DISPLACEMENT_TO_OPERAND(RBP, return_value_dips));

    emit_add(&jit_compiler->native_compiled_code, RSP_OPERAND, IMMEDIATE_TO_OPERAND(sizeof(lox_value_t)));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_GLOBAL_LENGTH);
}

//True value:  0x7ffc000000000003
//False value: 0x7ffc000000000002
//value1 = value0 + ((TRUE - value0) + (FALSE - value0))
//value1 = - value0 + TRUE + FALSE
//value1 = (TRUE - value) + FALSE
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

    if(slot > jit_compiler->last_stack_slot_allocated){
        uint8_t diff = slot - jit_compiler->last_stack_slot_allocated;
        uint8_t stack_grow = diff * sizeof(lox_value_t);
        emit_sub(&jit_compiler->native_compiled_code, IMMEDIATE_TO_OPERAND(stack_grow), RSP_OPERAND);
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
             DISPLACEMENT_TO_OPERAND(RBP, - offset_local_from_rbp)); //Stack grows to lower address

    record_compiled_bytecode(jit_compiler, instruction_index, OP_GET_LOCAL_LENGTH);
}

static void print(struct jit_compiler * jit_compiler) {
    register_t to_print_register_arg = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = call_external_c_function(
            &jit_compiler->native_compiled_code,
            (uint64_t) &print_lox_value,
            1,
            REGISTER_TO_OPERAND(to_print_register_arg));

    record_compiled_bytecode(jit_compiler, instruction_index, OP_PRINT_LENGTH);
}

static void jump_if_false(struct jit_compiler * jit_compiler, uint16_t jump_offset) {
    register_t lox_boolean_value = peek_register_allocator(&jit_compiler->register_allocator);
    register_t register_true_lox_value = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t first_instructino_index = emit_mov(&jit_compiler->native_compiled_code,
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
    record_compiled_bytecode(jit_compiler, first_instructino_index, OP_JUMP_IF_FALSE_LENGTH);
}

static void jump(struct jit_compiler * jit_compiler, uint16_t offset) {
    uint16_t jmp_index = emit_near_jmp(&jit_compiler->native_compiled_code, 0); //We don't know the offset of where to jump

    record_pending_jump_to_patch(jit_compiler, jmp_index, offset, 1); //jmp takes only 1 byte as opcode
    record_compiled_bytecode(jit_compiler, jmp_index, OP_JUMP_LENGTH);
}

static void pop(struct jit_compiler * jit_compiler) {
    pop_register_allocator(&jit_compiler->register_allocator);
    record_compiled_bytecode(jit_compiler, NATIVE_INDEX_IN_NEXT_SLOT, OP_POP_LENGTH);
}

static void loop(struct jit_compiler * jit_compiler, uint16_t bytecode_backward_jump) {
    uint16_t bytecode_index_to_jump = CURRENT_BYTECODE_INDEX(jit_compiler) - bytecode_backward_jump;
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

// a / b
static void division(struct jit_compiler * jit_compiler) {
    register_t b = pop_register_allocator(&jit_compiler->register_allocator);
    register_t a = pop_register_allocator(&jit_compiler->register_allocator);

    uint16_t mov_index = emit_mov(&jit_compiler->native_compiled_code,
                                  RAX_OPERAND,
                                  REGISTER_TO_OPERAND(a));

    emit_idiv(&jit_compiler->native_compiled_code, REGISTER_TO_OPERAND(b));

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

    //In x64 the max size of the immediate is 32 bit, QUIET_FLOAT_NAN is 64 bit.
    //We need to store QUIET_FLOAT_NAN in 64 bit register to later be able to or with register_casted_value
    emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(register_quiet_float_nan),
             IMMEDIATE_TO_OPERAND(QUIET_FLOAT_NAN));

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
//AS_OBJECT(value) ((struct object *) (uintptr_t)((value) & ~(FLOAT_SIGN_BIT | QUIET_FLOAT_NAN)))
//Allocates & deallocates new register
static uint16_t cast_lox_object_to_ptr(struct jit_compiler * jit_compiler, register_t lox_object_ptr) {
    //~(FLOAT_SIGN_BIT | QUIET_FLOAT_NAN)
    register_t not_fsb_qfn_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(not_fsb_qfn_reg),
             IMMEDIATE_TO_OPERAND(~(FLOAT_SIGN_BIT | QUIET_FLOAT_NAN)));

    emit_and(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(lox_object_ptr),
             REGISTER_TO_OPERAND(not_fsb_qfn_reg));

    pop_register_allocator(&jit_compiler->register_allocator);

    return instruction_index;
}

//Assembly implementation of TO_LOX_VALUE_OBJECT defined in types.h
//TO_LOX_VALUE_OBJECT(value) (FLOAT_SIGN_BIT | QUIET_FLOAT_NAN | (lox_value_t) value)
//Allocates & deallocates new register
static uint16_t cast_ptr_to_lox_object(struct jit_compiler * jit_compiler, register_t lox_object_ptr) {
    //FLOAT_SIGN_BIT | QUIET_FLOAT_NAN
    register_t or_fsb_qfn_reg = push_register_allocator(&jit_compiler->register_allocator);

    uint16_t instruction_index = emit_mov(&jit_compiler->native_compiled_code,
             REGISTER_TO_OPERAND(or_fsb_qfn_reg),
             IMMEDIATE_TO_OPERAND(FLOAT_SIGN_BIT | QUIET_FLOAT_NAN));

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