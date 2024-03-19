#include "vm.h"

extern struct trie_list * compiled_packages;
extern void check_gc_on_safe_point_alg();

__thread struct vm_thread * self_thread;
const uint8_t eof = OP_EOF;
struct vm current_vm;

static void setup_package_execution(struct package * package);
static void push_stack_vm(lox_value_t value);
static lox_value_t pop_stack_vm();
static double pop_and_check_number();
static bool check_boolean();
static interpret_result_t run();
static void print_stack();
static void runtime_error(char * format, ...);
static lox_value_t values_equal(lox_value_t a, lox_value_t b);
static inline lox_value_t peek(int index_from_top);
static void adition();
static inline struct call_frame * get_current_frame();
static void define_global(struct call_frame * current_callframe);
static void get_global(struct call_frame * current_frame);
static void set_global(struct call_frame * current_frame);
static void set_local(struct call_frame * current_frame);
static void get_local(struct call_frame * current_frame);
static void jump_if_false(struct call_frame * current_frame);
static void jump(struct call_frame * current_frame);
static void loop(struct call_frame * current_frame);
static void call(struct call_frame * current_frame);
static void return_function(struct call_frame * function_to_return_frame);
static void call_function(struct function_object * function, int n_args, bool is_parallel);
static void print_frame_stack_trace();
static void initialize_struct(struct call_frame * call_frame);
static void get_struct_field(struct call_frame * current_frame);
static void set_struct_field(struct call_frame * current_frame);
static void print();
static void enter_package();
static void initialize_package(struct package * package_to_initialize);
static void exit_package();
static void restore_prev_package_execution();
static void setup_native_functions();
static void setup_call_frame_function(struct vm_thread * thread, struct function_object * function);
static void setup_enter_package(struct package * package_to_enter);
static bool restore_prev_call_frame();
static void create_root_thread();
static void start_child_thread(struct function_object * thread_entry_point_func);
static int add_child_to_parent_list(struct vm_thread * new_child_thread);
static void copy_stack_from_esp(struct vm_thread * from, struct vm_thread * to, int n);
static void * run_thread_entrypoint(void * thread_ptr);
static bool some_child_thread_running(struct vm_thread * thread);
static void terminate_self_thread();
static void thread_on_safe_point();
static void enter_monitor_vm(struct call_frame * call_frame);
static void exit_monitor_vm(struct call_frame * call_frame);
static int try_add_child_to_parent_list(struct vm_thread * new_child_thread);
static void initialize_array(struct call_frame * call_frame);
static void get_array_element(struct call_frame * call_frame);
static void set_array_element(struct call_frame * call_frame);
static void fast_16_const(struct call_frame * call_frame);

#define READ_BYTE(frame) (*frame->pc++)
#define READ_U16(frame) \
    (frame->pc += 2, (uint16_t)((frame->pc[-2] << 8) | frame->pc[-1]))
#define READ_CONSTANT(frame) (frame->function->chunk.constants.values[READ_BYTE(frame)])
#define BINARY_OP(op) \
    do { \
        double b = pop_and_check_number(); \
        double a = pop_and_check_number(); \
        push_stack_vm(TO_LOX_VALUE_NUMBER(a op b)); \
    }while(false);

#define COMPARATION_OP(op) \
    do { \
        double b = pop_and_check_number(); \
        double a = pop_and_check_number(); \
        push_stack_vm(TO_LOX_VALUE_BOOL(a op b)); \
    }while(false);

interpret_result_t interpret_vm(struct compilation_result compilation_result) {
    if(!compilation_result.success){
        return INTERPRET_COMPILE_ERROR;
    }

    //By doing this, we enforce that no other package can call the main package
    compilation_result.compiled_package->state = INITIALIZING;

    create_root_thread();

    setup_package_execution(compilation_result.compiled_package);

    self_thread->esp += compilation_result.local_count;

    return run();
}

static void * run_thread_entrypoint(void * thread_ptr) {
    struct vm_thread * thread = (struct vm_thread *) thread_ptr;
    thread->native_thread = pthread_self();
    self_thread = thread;
    self_thread->state = THREAD_RUNNABLE;

    run();

    terminate_self_thread();

    return NULL;
}

static void terminate_self_thread() {
    if(some_child_thread_running(self_thread)) {
        runtime_error("Cannot end execution while some child thread still running");
    }

    self_thread->terminated_state = THREAD_TERMINATED_PENDING_GC;
    self_thread->state = THREAD_TERMINATED;

    atomic_fetch_sub(&current_vm.number_current_threads, 1);

    thread_on_safe_point();

    pthread_exit(NULL);
}

static bool some_child_thread_running(struct vm_thread * thread) {
    for(int i = 0; i < MAX_THREADS_PER_THREAD; i++) {
        struct vm_thread * current_thread = thread->children[i];

        if(current_thread != NULL && current_thread->state < THREAD_TERMINATED) { //They are in order
            return true;
        }
    }

    return false;
}

static interpret_result_t run() {
    struct call_frame * current_frame = get_current_frame();

    thread_on_safe_point();

    for(;;) {
        switch (READ_BYTE(current_frame)) {
            case OP_RETURN: return_function(current_frame); current_frame = get_current_frame(); break;
            case OP_CONSTANT: push_stack_vm(READ_CONSTANT(current_frame)); break;
            case OP_NEGATE: push_stack_vm(TO_LOX_VALUE_NUMBER(-pop_and_check_number())); break;
            case OP_ADD: adition(); break;
            case OP_SUB: BINARY_OP(-) break;
            case OP_MUL: BINARY_OP(*) break;
            case OP_DIV: BINARY_OP(/) break;
            case OP_GREATER: COMPARATION_OP(>) break;
            case OP_LESS: COMPARATION_OP(<) break;
            case OP_FALSE: push_stack_vm(TO_LOX_VALUE_BOOL(false)); break;
            case OP_TRUE: push_stack_vm(TO_LOX_VALUE_BOOL(true)); break;
            case OP_NIL: push_stack_vm(NIL_VALUE()); break;
            case OP_NOT: push_stack_vm(TO_LOX_VALUE_BOOL(!check_boolean())); break;
            case OP_EQUAL: push_stack_vm(values_equal(pop_stack_vm(), pop_stack_vm())); break;
            case OP_PRINT: print(); break; //Checks for start gc signal
            case OP_POP: pop_stack_vm(); break;
            case OP_DEFINE_GLOBAL: define_global(current_frame); break; //Checks for start gc signal
            case OP_GET_GLOBAL: get_global(current_frame); break;
            case OP_SET_GLOBAL: set_global(current_frame); break;
            case OP_GET_LOCAL: get_local(current_frame); break;
            case OP_JUMP_IF_FALSE: jump_if_false(current_frame); break; //Checks for start gc signal
            case OP_JUMP: jump(current_frame); break; //Checks for start gc signal
            case OP_SET_LOCAL: set_local(current_frame); break;
            case OP_LOOP: loop(current_frame); break; //Checks for start gc signal
            case OP_CALL: call(current_frame); current_frame = get_current_frame(); break; //Checks for start gc signal
            case OP_INITIALIZE_STRUCT: initialize_struct(current_frame); break;
            case OP_GET_STRUCT_FIELD: get_struct_field(current_frame); break;
            case OP_SET_STRUCT_FIELD: set_struct_field(current_frame); break;
            case OP_ENTER_PACKAGE: enter_package(); current_frame = get_current_frame(); break;
            case OP_EXIT_PACKAGE: exit_package(); current_frame = get_current_frame(); break;
            case OP_ENTER_MONITOR: enter_monitor_vm(current_frame); break;
            case OP_EXIT_MONITOR: exit_monitor_vm(current_frame); break;
            case OP_INITIALIZE_ARRAY: initialize_array(current_frame); break;
            case OP_GET_ARRAY_ELEMENT: get_array_element(current_frame); break;
            case OP_SET_ARRAY_ELEMENT: set_array_element(current_frame); break;
            case OP_FAST_CONST_8: push_stack_vm(TO_LOX_VALUE_NUMBER(READ_BYTE(current_frame))); break;
            case OP_FAST_CONST_16: push_stack_vm(TO_LOX_VALUE_NUMBER(READ_U16(current_frame))); break;
            case OP_CONST_1: push_stack_vm(TO_LOX_VALUE_NUMBER(1)); break;
            case OP_CONST_2: push_stack_vm(TO_LOX_VALUE_NUMBER(2)); break;
            case OP_EOF: return INTERPRET_OK;
            default:
                perror("Unhandled bytecode op\n");
                return INTERPRET_RUNTIME_ERROR;
        }
    }
}

static void enter_monitor_vm(struct call_frame * call_frame) {
    struct function_object * function = call_frame->function;
    int monitor_number_to_enter = READ_BYTE(call_frame);

    call_frame->monitors_entered[call_frame->last_monitor_entered_index++] = monitor_number_to_enter;

    struct monitor * monitor_to_enter = &function->monitors[monitor_number_to_enter];

    set_self_thread_waiting();

    enter_monitor(monitor_to_enter);

    set_self_thread_runnable();
}

static void exit_monitor_vm(struct call_frame * call_frame) {
    int monitor_number_to_exit = call_frame->monitors_entered[--call_frame->last_monitor_entered_index];
    struct function_object * function = call_frame->function;
    struct monitor * monitor_to_exit = &function->monitors[monitor_number_to_exit];

    exit_monitor(monitor_to_exit);
}

static void enter_package() {
    struct package_object * package_object = (struct package_object *) AS_OBJECT(pop_stack_vm());
    struct package * package = package_object->package;

    pthread_mutex_lock(&package->state_mutex);

    switch (package->state) {
        case READY_TO_USE: break;
        case INITIALIZING: runtime_error("Found cyclical dependency with package %s", package->name);
        case PENDING_INITIALIZATION: initialize_package(package); break;
        default: runtime_error("Unexpected package state Found bug in VM with package %s", package->name);
    }

    pthread_mutex_unlock(&package->state_mutex);

    setup_enter_package(package);
}

// We only want to keep the constants of the package_to_enter, in case we set or get a global variable
static void setup_enter_package(struct package * package_to_enter) {
    struct call_frame * prev_frame = get_current_frame();
    struct call_frame * new_frame = &self_thread->frames[self_thread->frames_in_use++];

    new_frame->pc = prev_frame->pc;
    new_frame->function = package_to_enter->main_function;
    new_frame->slots = prev_frame->slots;

    push_stack(&self_thread->package_stack, self_thread->current_package);
    self_thread->current_package = package_to_enter;
}

static void exit_package() {
    restore_prev_package_execution();
    get_current_frame()->pc = self_thread->frames[self_thread->frames_in_use].pc;
}

static void initialize_package(struct package * package_to_initialize) {
    setup_package_execution(package_to_initialize);

    package_to_initialize->state = INITIALIZING;

    if(run() == INTERPRET_RUNTIME_ERROR) {
        runtime_error("Error while interpreting package %s", package_to_initialize->name);
    }

    package_to_initialize->state = READY_TO_USE;

    restore_prev_package_execution();
}

static void setup_package_execution(struct package * package) {
    push_stack(&self_thread->package_stack, self_thread->current_package);
    self_thread->current_package = package;

    if(package->state != READY_TO_USE) {
        init_hash_table(&package->global_variables);
        setup_native_functions();

        setup_call_frame_function(self_thread, package->main_function);
    }
}

static void restore_prev_package_execution() {
    self_thread->current_package = pop_stack(&self_thread->package_stack);
    restore_prev_call_frame();
}

static void adition() {
    if(IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double a = AS_NUMBER(pop_stack_vm());
        double b = AS_NUMBER(pop_stack_vm());
        push_stack_vm(TO_LOX_VALUE_NUMBER(a + b));
        return;
    }

    lox_value_t b_value = pop_stack_vm();
    lox_value_t a_value = pop_stack_vm();
    char * a_chars = to_string(a_value);
    char * b_chars = to_string(b_value);
    size_t a_length = strlen(a_chars);
    size_t b_length = strlen(b_chars);

    size_t new_length = a_length + b_length; //Include \0
    char * concatenated = malloc(sizeof(char) * (new_length + 1));
    memcpy(concatenated, a_chars, a_length);
    memcpy(concatenated + a_length, b_chars, b_length);
    concatenated[new_length] = '\0';

    if(IS_NUMBER(a_value)){
        free(a_chars);
    }
    if(IS_NUMBER(b_value)){
        free(b_chars);
    }

    struct string_pool_add_result add_result = add_to_global_string_pool(concatenated, new_length);

    push_stack_vm(TO_LOX_VALUE_OBJECT(add_result.string_object));

    if(add_result.created_new) {
        int total_bytes_allocated = sizeof_heap_allocated_lox_object(&add_result.string_object->object);
        add_object_to_heap(self_thread->gc_info, &add_result.string_object->object, total_bytes_allocated);
    } else {
        free(concatenated);
    }
}

static void define_global(struct call_frame * current_callframe) {
    struct string_object * name = AS_STRING_OBJECT(READ_CONSTANT(current_callframe));
    put_hash_table(&self_thread->current_package->global_variables, name, pop_stack_vm());
    thread_on_safe_point();
}

static void get_global(struct call_frame * current_frame) {
    struct string_object * variable_name = AS_STRING_OBJECT(READ_CONSTANT(current_frame));
    lox_value_t variable_value;
    if(!get_hash_table(&self_thread->current_package->global_variables, variable_name, &variable_value)) {
        runtime_error("Undefined variable %s.", variable_name->chars);
    }

    push_stack_vm(variable_value);
}

static void set_global(struct call_frame * current_frame) {
    struct string_object * variable_name = AS_STRING_OBJECT(READ_CONSTANT(current_frame));

    //Assigment is an expression
    if(!put_if_present_hash_table(&self_thread->current_package->global_variables, variable_name, peek(0))) {
        runtime_error("Cannot assign value to undeclared variable %s", variable_name->chars);
    }
}

static void set_local(struct call_frame * current_frame) {
    uint8_t slot = READ_BYTE(current_frame);
    lox_value_t value = peek(0);
    current_frame->slots[slot] = value; //Assigment is an expression
}

static void get_local(struct call_frame * current_frame) {
    uint8_t slot = READ_BYTE(current_frame);
    lox_value_t value = current_frame->slots[slot];
    push_stack_vm(value);
}

static void call(struct call_frame * current_frame) {
    int n_args = READ_BYTE(current_frame);
    bool is_parallel = READ_BYTE(current_frame);

    lox_value_t callee = peek(n_args);

    if(!IS_OBJECT(callee)){
        runtime_error("Cannot call");
    }

    switch (AS_OBJECT(callee)->type) {
        case OBJ_FUNCTION: {
            call_function(AS_FUNCTION(callee), n_args, is_parallel);
            break;
        }
        case OBJ_NATIVE: {
            if(is_parallel) {
                runtime_error("Cannot call parallel in native functions");
            }

            struct native_object * native_function_object = TO_NATIVE(callee);
            native_fn native_function = native_function_object->native_fn;

            lox_value_t result = native_function(n_args, self_thread->esp - n_args);
            self_thread->esp -= n_args + 1;
            push_stack_vm(result);

            break;
        }
        default:
            runtime_error("Cannot call");
    }

    thread_on_safe_point();
}

static void call_function(struct function_object * function, int n_args, bool is_parallel) {
    if(n_args != function->n_arguments){
        runtime_error("Cannot call %s with %i args. Required %i nº args", function->name->chars, n_args, function->n_arguments);
    }
    if(self_thread->frames_in_use >= FRAME_MAX){
        runtime_error("Stack overflow. Max allowed frames: %i", FRAME_MAX);
    }

    if(!is_parallel) {
        setup_call_frame_function(self_thread, function);
    } else {
        start_child_thread(function);
    }
}

static void print() {
    lox_value_t value = pop_stack_vm();

#ifdef VM_TEST
    current_vm.log[current_vm.log_entries_in_use++] = to_string(value);
#else
    print_value(value);
#endif

    thread_on_safe_point();
}

static void return_function(struct call_frame * function_to_return_frame) {
    lox_value_t returned_value = pop_stack_vm();

    bool last_frame = restore_prev_call_frame();

    if(!last_frame) {
        self_thread->esp = function_to_return_frame->slots;
    }

    push_stack_vm(returned_value);
}

static void jump(struct call_frame * current_frame) {
    current_frame->pc += READ_U16(current_frame);
    thread_on_safe_point();
}

static void initialize_array(struct call_frame * call_frame) {
    uint8_t n_elements = READ_BYTE(call_frame);
    struct array_object * array = alloc_array_object(n_elements);

    for(int i = 0; i < n_elements; i++) {
        lox_value_t value = pop_stack_vm();
        int index = n_elements - i - 1;

        set_element_array(array, index, value);
    }

    int total_bytes_allocated = sizeof_heap_allocated_lox_object(&array->object);
    add_object_to_heap(self_thread->gc_info, &array->object, total_bytes_allocated);

    push_stack_vm(TO_LOX_VALUE_OBJECT(array));
}

static void get_array_element(struct call_frame * call_frame) {
    uint16_t array_index = READ_U16(call_frame);
    struct array_object * array = (struct array_object *) AS_OBJECT(pop_stack_vm());

    if(array_index >= array->values.in_use) {
        runtime_error("Index out of bounds");
    }

    push_stack_vm(array->values.values[array_index]);
}

static void set_array_element(struct call_frame * call_frame) {
    uint16_t array_index = READ_U16(call_frame);
    struct array_object * array = (struct array_object *) AS_OBJECT(pop_stack_vm());
    lox_value_t new_value = pop_stack_vm();

    if(array_index >= array->values.in_use) {
        runtime_error("Index out of bounds");
    }

    array->values.values[array_index] = new_value;
}

static void initialize_struct(struct call_frame * call_frame) {
    struct struct_instance_object * struct_instance = alloc_struct_instance_object();
    struct struct_definition_object * struct_definition = (struct struct_definition_object *) AS_OBJECT(READ_CONSTANT(call_frame));
    int n_fields = struct_definition->n_fields;

    struct_instance->definition = struct_definition;

    for(int i = 0; i < n_fields; i++) {
        struct string_object * field_name = struct_definition->field_names[struct_definition->n_fields - i - 1];
        put_hash_table(&struct_instance->fields, field_name, pop_stack_vm());
    }

    push_stack_vm(TO_LOX_VALUE_OBJECT(struct_instance));

    int total_bytes_allocated = sizeof_heap_allocated_lox_object(&struct_instance->object);
    add_object_to_heap(self_thread->gc_info, &struct_instance->object, total_bytes_allocated);
}

static void get_struct_field(struct call_frame * call_frame) {
    struct struct_instance_object * instance = (struct struct_instance_object *) AS_OBJECT(pop_stack_vm());
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(call_frame));

    lox_value_t field_value;
    if(!get_hash_table(&instance->fields, field_name, &field_value)) {
        runtime_error("Undefined field %s", field_name->chars);
    }

    push_stack_vm(field_value);
}

static void set_struct_field(struct call_frame * call_frame) {
    lox_value_t new_value = pop_stack_vm();
    struct struct_instance_object * instance = (struct struct_instance_object *) AS_OBJECT(pop_stack_vm());
    struct string_object * field_name = (struct string_object *) AS_OBJECT(READ_CONSTANT(call_frame));

    if(!put_if_present_hash_table(&instance->fields, field_name, new_value)) {
        runtime_error("Undefined field %s", field_name->chars);
    }
}

static void jump_if_false(struct call_frame * call_frame) {
    if(!cast_to_boolean(peek(0))){
        int total_opcodes_to_jump_if_false = READ_U16(call_frame);
        call_frame->pc += total_opcodes_to_jump_if_false;
    } else {
        call_frame->pc += 2;
    }

    thread_on_safe_point();
}

static void loop(struct call_frame * call_frame) {
    uint16_t val = READ_U16(call_frame);
    call_frame->pc -= val;
    thread_on_safe_point();
}

static inline lox_value_t peek(int index_from_top) {
    return *(self_thread->esp - 1 - index_from_top);
}

static double pop_and_check_number() {
    lox_value_t value = pop_stack_vm();

    if(IS_NUMBER(value)) {
        double d = AS_NUMBER(value);
        return d;
    } else {
        runtime_error("Operand must be a number.");
        return -1; //Unreachable
    }
}

static bool check_boolean() {
    lox_value_t value = pop_stack_vm();
    if(IS_BOOL(value)) {
        return AS_BOOL(value);
    } else {
        runtime_error("Operand must be a boolean.");
        return false; //Compiler doest warn me
    }
}

static lox_value_t values_equal(lox_value_t a, lox_value_t b) {
#ifdef NAN_BOXING
    return TO_LOX_VALUE_BOOL(a == b);
#else
    if(a.type != b.type) {
        return TO_LOX_VALUE_BOOL(false);
    }

    switch (a.type) {
        case VAL_NIL: return TO_LOX_VALUE_BOOL(true);
        case VAL_NUMBER: return TO_LOX_VALUE_BOOL(a.as.number == b.as.number);
        case VAL_BOOL: return TO_LOX_VALUE_BOOL(a.as.boolean == b.as.boolean);
        case VAL_OBJ: return TO_LOX_VALUE_BOOL(AS_STRING_OBJECT(a)->chars == AS_STRING_OBJECT(b)->chars);
        default:
            runtime_error("Operator '==' not supported for that type");
            return TO_LOX_VALUE_BOOL(false); //Unreachable, runtime_error executes exit()
    }
#endif
}

static void push_stack_vm(lox_value_t value) {
    *self_thread->esp = value;
    self_thread->esp++;
}

static lox_value_t pop_stack_vm() {
    auto val = *--self_thread->esp;
    return val;
}

void start_vm() {
    current_vm.gc = alloc_gc();
    current_vm.number_current_threads = 0;
    current_vm.last_thread_id = 0;

#ifdef VM_TEST
    current_vm.log_entries_in_use = 0;
#endif
}

void stop_vm() {
    free_trie_list(compiled_packages);
    clear_stack(&self_thread->package_stack);
}

static void runtime_error(char * format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    struct call_frame * current_frame = get_current_frame();

    size_t instruction = ((uint8_t *) current_frame->pc) - current_frame->function->chunk.code - 1;
    int line = current_frame->function->chunk.lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    print_frame_stack_trace();

    exit(1);
}

static void print_frame_stack_trace() {
    for (int i = self_thread->frames_in_use - 1; i >= 0; i--) {
        struct call_frame * frame = &self_thread->frames[i];
        struct function_object * function = frame->function;

        size_t instruction = frame->pc - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
}

static void print_stack() {
    printf("\t");
    for(lox_value_t * value = self_thread->stack; value < self_thread->esp; value++)  {
        printf("[");
        print_value(*value);
        printf("]");
    }
    printf("\n");
}

static inline struct call_frame * get_current_frame() {
    return &self_thread->frames[self_thread->frames_in_use - 1];
}

static void setup_native_functions() {
    define_native("selfThreadId", self_thread_id_native, false);
    define_native("sleep", sleep_ms_native, true);
    define_native("clock", clock_native, false);
    define_native("forceGC", force_gc, false);
    define_native("join", join_native, true);
}

void define_native(char * function_name, native_fn native_function, bool is_blocking) {
    struct string_object * function_name_obj = copy_chars_to_string_object(function_name, strlen(function_name));
    struct native_object * native_object = alloc_native_object(native_function, is_blocking);

    put_hash_table(&self_thread->current_package->global_variables, function_name_obj, TO_LOX_VALUE_OBJECT(native_object));
}

static void setup_call_frame_function(struct vm_thread * thread, struct function_object * function) {
    struct call_frame * new_frame = &thread->frames[thread->frames_in_use++];

    new_frame->function = function;
    new_frame->pc = function->chunk.code;
    new_frame->slots = thread->esp - function->n_arguments - 1;
    new_frame->last_monitor_entered_index = 0;
}

static bool restore_prev_call_frame() {
    self_thread->frames_in_use--;

    bool last_frame = self_thread->frames_in_use == 0;

    //We have hit end of execution of a thread (thread entrypoint are functions)
    if(last_frame) {
        struct call_frame * eof_call_frame = &self_thread->frames[self_thread->frames_in_use++];
        eof_call_frame->pc = &eof;
    }

    return last_frame;
}

static void create_root_thread() {
    struct vm_thread * root_thread = alloc_vm_thread();
    root_thread->thread_id = atomic_fetch_add(&current_vm.last_thread_id, 1);
    root_thread->native_thread = pthread_self();
    root_thread->esp = root_thread->stack;
    root_thread->state = THREAD_RUNNABLE;
    root_thread->gc_info = alloc_gc_thread_info();
    root_thread->gc_info->gc_global_info = current_vm.gc;

    current_vm.root = root_thread;
    current_vm.number_current_threads += 1;

    self_thread = root_thread;
}

static void start_child_thread(struct function_object * thread_entry_point_func) {
    struct vm_thread * new_thread = alloc_vm_thread();
    new_thread->thread_id = atomic_fetch_add(&current_vm.last_thread_id, 1);
    new_thread->state = THREAD_NEW;
    new_thread->current_package = self_thread->current_package;
    new_thread->gc_info = alloc_gc_thread_info();
    new_thread->gc_info->gc_global_info = current_vm.gc;
    new_thread->parent = self_thread;

    add_child_to_parent_list(new_thread);
    copy_stack_from_esp(self_thread, new_thread, thread_entry_point_func->n_arguments);
    setup_call_frame_function(new_thread, thread_entry_point_func);

    atomic_fetch_add(&current_vm.number_current_threads, 1);

    pthread_create(&new_thread->native_thread, NULL, run_thread_entrypoint, new_thread);
}

static void copy_stack_from_esp(struct vm_thread * from, struct vm_thread * to, int n) {
    for(int i = 0; i < n; i++){
        to->stack[i] = from->esp[- i - 1];
        to->esp += 1;
    }
}

static int add_child_to_parent_list(struct vm_thread * new_child_thread) {
    int index;

    if((index = try_add_child_to_parent_list(new_child_thread)) != -1){
        return index;
    }

    try_start_gc(self_thread->gc_info);

    //The gc will remove termianted threads
    if((index = try_add_child_to_parent_list(new_child_thread)) != -1) {
        return index;
    }

    runtime_error("Exceeded max number of child threads_race_conditions %i per thread", MAX_THREADS_PER_THREAD);

    return -1;
}

static int try_add_child_to_parent_list(struct vm_thread * new_child_thread) {
    for(int i = 0; i < MAX_THREADS_PER_THREAD; i++){
        struct vm_thread * current_thread_slot = self_thread->children[i];

        if(current_thread_slot == NULL) {
            self_thread->children[i] = new_child_thread;
            return i;
        } else if(current_thread_slot != NULL &&
                current_thread_slot->state == THREAD_TERMINATED &&
                current_thread_slot->terminated_state == THREAD_TERMINATED_GC_DONE) {
            self_thread->children[i] = NULL;
            free(current_thread_slot);
        }
    }

    return -1;
}

static void thread_on_safe_point() {
    check_gc_on_safe_point_alg();
}

void set_self_thread_waiting() {
    self_thread->state = THREAD_WAITING;
    //Avoid race condition when garbage collection starts
    atomic_fetch_add(&current_vm.number_waiting_threads, 1);
    thread_on_safe_point();
}

void set_self_thread_runnable() {
    self_thread->state = THREAD_RUNNABLE;
    atomic_fetch_sub(&current_vm.number_waiting_threads, 1);
    thread_on_safe_point();
}

void on_gc_finished_vm(struct gc_result result) {
#ifdef VM_TEST
    current_vm.last_gc_result = result;
#endif
}