#include "graphviz_visualizer.h"

struct graphviz_visualizer {
    FILE * file;

    struct ssa_ir ssa_ir;
    struct function_object * function;
    //Set of pointers of struct ssa_block visited
    struct u64_set visited_blocks;
    //Last contorl node graph id by block pointer
    struct u64_hash_table block_generated_graph_by_block;

    int next_block_id;
    int next_control_node_id;
    int next_data_node_id;
};

//We use a union so that we can store this struct in block_generated_graph_by_block as u64 easily
struct block_graph_generated {
    union {
        struct {
            int first_control_node_id;
            int last_control_node_id;
        } value;
        uint64_t u64_value;
    };
};

static void generate_graph_and_write(struct graphviz_visualizer *, struct ssa_block *);
static struct block_graph_generated generate_block_graph(struct graphviz_visualizer *, struct ssa_block *);
static int generate_control_node_graph(struct graphviz_visualizer *, struct ssa_control_node *);
static int generate_data_node_graph(struct graphviz_visualizer *, struct ssa_data_node *);
static struct block_graph_generated generate_blocks_graph(struct graphviz_visualizer *, struct ssa_block * first);

static char * unary_operator_to_string(ssa_unary_operator_type_t);
static char * binary_operator_to_string(bytecode_t);

struct graphviz_visualizer create_graphviz_visualizer(char *, struct arena_lox_allocator *, struct function_object *);
void append_new_line_graphviz_file(struct graphviz_visualizer *, char * to_append);
void append_new_block_graphviz_file(struct graphviz_visualizer *, int);
void append_new_data_node_graphviz_file(struct graphviz_visualizer *, char *, int);
void link_data_data_node_graphviz_file(struct graphviz_visualizer *, int from, int to);
void link_control_data_node_graphviz_file(struct graphviz_visualizer *, int from, int to);
void link_control_control_node_graphviz_file(struct graphviz_visualizer *, int from, int to);
void link_block_block_node_graphviz_file(struct graphviz_visualizer *, struct block_graph_generated, struct block_graph_generated);
void append_new_control_node_graphviz_file(struct graphviz_visualizer *, char *, int);

void generate_ssa_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_ssa_graphviz_t phase,
        char * path
) {
    struct arena arena;
    init_arena(&arena);
    struct arena_lox_allocator ssa_node_allocator = to_lox_allocator_arena(arena);
    struct graphviz_visualizer graphviz_visualizer = create_graphviz_visualizer(path, &ssa_node_allocator, function);

    switch (phase) {
        case NO_PHIS_PHASE_SSA_GRAPHVIZ: {
            struct ssa_block * ssa_block = create_ssa_ir_no_phis(package, function,
                create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator);

            generate_graph_and_write(&graphviz_visualizer, ssa_block);
            break;
        }
        case PHIS_INSERTED_PHASE_SSA_GRAPHVIZ: {
            struct ssa_block * ssa_block = create_ssa_ir_no_phis(package, function,
                create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator);
            insert_ssa_ir_phis(ssa_block, &ssa_node_allocator);

            generate_graph_and_write(&graphviz_visualizer, ssa_block);
            break;
        }
        case PHIS_OPTIMIZED_PHASE_SSA_GRAPHVIZ: {
            struct ssa_block * ssa_block = create_ssa_ir_no_phis(package, function,
                create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator);
            struct phi_insertion_result phi_insertion_result = insert_ssa_ir_phis(ssa_block, &ssa_node_allocator);
            optimize_ssa_ir_phis(ssa_block, &phi_insertion_result, &ssa_node_allocator);

            generate_graph_and_write(&graphviz_visualizer, ssa_block);
            break;
        }
        case SPARSE_CONSTANT_PROPAGATION_PHASE_SSA_GRAPHVIZ: {
            struct ssa_ir ssa_ir = create_ssa_ir(package, function, create_bytecode_list(function->chunk,
                &ssa_node_allocator.lox_allocator));
            perform_sparse_constant_propagation(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case ALL_PHASE_SSA_GRAPHVIZ: {
            struct ssa_ir ssa_ir = create_ssa_ir(package, function, create_bytecode_list(function->chunk,
                &ssa_node_allocator.lox_allocator));
            perform_sparse_constant_propagation(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
    }
}

static void generate_graph_and_write(struct graphviz_visualizer * graphviz_visualizer, struct ssa_block * first_block) {
    append_new_line_graphviz_file(graphviz_visualizer, "digraph G {");
    generate_blocks_graph(graphviz_visualizer, first_block);
    append_new_line_graphviz_file(graphviz_visualizer, "}");
}

static struct block_graph_generated generate_blocks_graph(struct graphviz_visualizer * visualizer, struct ssa_block * first_block) {
    struct block_graph_generated first_block_graph_data = generate_block_graph(visualizer, first_block);

    switch (first_block->type_next_ssa_block) {
        case TYPE_NEXT_SSA_BLOCK_SEQ: {
            struct block_graph_generated next_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.next);
            link_block_block_node_graphviz_file(visualizer, first_block_graph_data, next_block_graph_data);
            break;
        }
        case TYPE_NEXT_SSA_BLOCK_LOOP: {
            struct block_graph_generated loop_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.loop);
            link_block_block_node_graphviz_file(visualizer, first_block_graph_data, loop_block_graph_data);
            break;
        }
        case TYPE_NEXT_SSA_BLOCK_BRANCH: {
            struct block_graph_generated branch_false_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.branch.false_branch);
            struct block_graph_generated branch_true_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.branch.true_branch);
            link_block_block_node_graphviz_file(visualizer, first_block_graph_data, branch_false_block_graph_data);
            link_block_block_node_graphviz_file(visualizer, first_block_graph_data, branch_true_block_graph_data);
            break;
        }
        case TYPE_NEXT_SSA_BLOCK_NONE:
            break;
    }

    return first_block_graph_data;
}

static struct block_graph_generated generate_block_graph(struct graphviz_visualizer * visualizer, struct ssa_block * block) {
    if(contains_u64_set(&visualizer->visited_blocks, (uint64_t) block)) {
        uint64_t block_generated_graph_by_block_u64 = (uint64_t) get_u64_hash_table(&visualizer->block_generated_graph_by_block, (uint64_t) block);
        return (struct block_graph_generated) {.u64_value = block_generated_graph_by_block_u64};
    }

    add_u64_set(&visualizer->visited_blocks, (uint64_t) block);

    append_new_block_graphviz_file(visualizer, visualizer->next_block_id++);
    int first_control_node_id = 0;
    int last_control_node_id = 0;
    for(struct ssa_control_node * current = block->first;; current = current->next){
        int control_node_id = generate_control_node_graph(visualizer, current);

        if(current == block->last){
            first_control_node_id = control_node_id;
        }
        if(current == block->last){
            last_control_node_id = control_node_id;
            break;
        }
    }
    append_new_line_graphviz_file(visualizer, "}");

    put_u64_hash_table(&visualizer->block_generated_graph_by_block, (uint64_t) block, (void *)last_control_node_id);

    return (struct block_graph_generated) { .value = { first_control_node_id, last_control_node_id }};
}

static int generate_control_node_graph(struct graphviz_visualizer * visualizer, struct ssa_control_node * node) {
    int self_control_node_id = visualizer->next_control_node_id++;
    switch (node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA: {
            struct ssa_data_node * data = (struct ssa_data_node *) node;
            int data_node_id = generate_data_node_graph(visualizer, data);

            append_new_control_node_graphviz_file(visualizer, "Data", self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_RETURN: {
            struct ssa_control_return_node * return_node = (struct ssa_control_return_node *) node;
            int data_node_id = generate_data_node_graph(visualizer, return_node->data);

            append_new_control_node_graphviz_file(visualizer, "Return", self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_PRINT: {
            struct ssa_control_print_node * print_node = (struct ssa_control_print_node *) node;
            int data_node_id = generate_data_node_graph(visualizer, print_node->data);

            append_new_control_node_graphviz_file(visualizer, "Print", self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR: {
            struct ssa_control_enter_monitor_node * enter_monitor = (struct ssa_control_enter_monitor_node *) node;
            char * node_desc = dynamic_format_string("EnterMonitor %i\n0x", enter_monitor->monitor_number, enter_monitor->monitor);

            append_new_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR: {
            struct ssa_control_exit_monitor_node * exit_monitor = (struct ssa_control_exit_monitor_node *) node;
            char * node_desc = dynamic_format_string("ExitMonitor %i\n0x", exit_monitor->monitor_number, exit_monitor->monitor);

            append_new_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL: {
            struct ssa_control_set_global_node * set_global = (struct ssa_control_set_global_node *) node;
            char * package_name = set_global->package->name;
            char * global_name = set_global->name->chars;
            char * node_desc = dynamic_format_string("SetGlobal\npackage: %s\nname: %s", package_name, global_name);

            int global_value_data_node_id = generate_data_node_graph(visualizer, set_global->value_node);
            append_new_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, global_value_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL: {
            struct ssa_control_set_local_node * set_local = (struct ssa_control_set_local_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, set_local->local_number);
            char * node_desc = dynamic_format_string("SetLocal %s", local_name);

            int local_value_data_node_id = generate_data_node_graph(visualizer, set_local->new_local_value);
            append_new_data_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, local_value_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD: {
            struct ssa_control_set_struct_field_node * set_struct_field = (struct ssa_control_set_struct_field_node *) node;
            char * node_desc = dynamic_format_string("SetStructField %s", set_struct_field->field_name->chars);

            int field_value_data_node_id = generate_data_node_graph(visualizer, set_struct_field->new_field_value);
            int struct_instance_data_node_id = generate_data_node_graph(visualizer, set_struct_field->instance);
            append_new_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, field_value_data_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, struct_instance_data_node_id);
            free(node_desc);
        }
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT: {
            struct ssa_control_set_array_element_node * set_array_element = (struct ssa_control_set_array_element_node *) node;
            char * node_desc = dynamic_format_string("SetArrayElement %i", set_array_element->index);

            int array_element_data_node_id = generate_data_node_graph(visualizer, set_array_element->new_element_value);
            int array_instance_data_node_id = generate_data_node_graph(visualizer, set_array_element->array);
            append_new_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, array_element_data_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, array_instance_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP: {
            struct ssa_control_conditional_jump_node * cond_jump = (struct ssa_control_conditional_jump_node *) node;
            char * node_desc = dynamic_format_string("ConditionalJump\nLoop condition: %i", cond_jump->loop_condition);

            int condition_data_node_id = generate_data_node_graph(visualizer, cond_jump->condition);
            append_new_control_node_graphviz_file(visualizer, node_desc, condition_data_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, condition_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME: {
            struct ssa_control_define_ssa_name_node * define_ssa_name = (struct ssa_control_define_ssa_name_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, define_ssa_name->ssa_name.value.local_number);
            char * node_desc = dynamic_format_string("DefineSSA %s %i\n", local_name, define_ssa_name->ssa_name.value.version);

            int ssa_name_value_data_node_id = generate_data_node_graph(visualizer, define_ssa_name->value);
            append_new_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            link_control_data_node_graphviz_file(visualizer, self_control_node_id, ssa_name_value_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP: {
            append_new_control_node_graphviz_file(visualizer, "Loop", self_control_node_id);
            break;
        }
    }

    return self_control_node_id;
}

static int generate_data_node_graph(struct graphviz_visualizer * visualizer, struct ssa_data_node * node) {
    int self_data_node_id = visualizer->next_data_node_id++;

    switch (node->type) {
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            int unary_value_node_id = generate_data_node_graph(visualizer, unary->operand);
            char * node_desc = dynamic_format_string("%s", unary_operator_to_string(unary->operator_type));

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, unary_value_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            int left_node_id = generate_data_node_graph(visualizer, binary->left);
            int right_node_id = generate_data_node_graph(visualizer, binary->right);
            char * node_desc = dynamic_format_string("%s", binary_operator_to_string(binary->operand));
            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);

            link_data_data_node_graphviz_file(visualizer, self_data_node_id, left_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, right_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call = (struct ssa_data_function_call_node *) node;
            int function_node_id = generate_data_node_graph(visualizer, call->function);

            append_new_data_node_graphviz_file(visualizer, "FunctionCall", self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, function_node_id);
            for (int i = 0; i < call->n_arguments; i++){
                int function_arg_node_id = generate_data_node_graph(visualizer, call->arguments[i]);
                link_data_data_node_graphviz_file(visualizer, self_data_node_id, function_arg_node_id);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            char * node_desc = dynamic_format_string("GetStructField %s", get_struct_field->field_name->chars);
            int struct_instance_node_id = generate_data_node_graph(visualizer, get_struct_field->instance_node);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, struct_instance_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT: {
            struct ssa_data_initialize_struct_node * initialize_struct = (struct ssa_data_initialize_struct_node *) node;
            char * node_desc = dynamic_format_string("InitializeStruct %s", initialize_struct->definition->name);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            for(int i = 0; i < initialize_struct->definition->n_fields; i++){
                int struct_field_node_id = generate_data_node_graph(visualizer, initialize_struct->fields_nodes[i]);
                link_data_data_node_graphviz_file(visualizer, self_data_node_id, struct_field_node_id);
            }
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT: {
            struct ssa_data_get_array_element_node * get_array_element = (struct ssa_data_get_array_element_node *) node;
            char * node_desc = dynamic_format_string("GetArrayElement %i", get_array_element->index);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            int array_instance_node_id = generate_data_node_graph(visualizer, get_array_element->instance);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, array_instance_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY: {
            struct ssa_data_initialize_array_node * initialize_array = (struct ssa_data_initialize_array_node *) node;
            char * node_desc = dynamic_format_string("InitializeArray %i", initialize_array->n_elements);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            for(int i = 0; i < initialize_array->n_elements && !initialize_array->empty_initialization; i++){
                int array_element_node_id = generate_data_node_graph(visualizer, initialize_array->elememnts_node[i]);
                link_data_data_node_graphviz_file(visualizer, self_data_node_id, array_element_node_id);
            }
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, phi->local_number);
            struct string_builder node_desc_string_builder;
            init_string_builder(&node_desc_string_builder, NATIVE_LOX_ALLOCATOR());
            append_string_builder(&node_desc_string_builder, "Phi (");
            FOR_EACH_VERSION_IN_PHI_NODE(phi, current_version) {
                char * to_append = dynamic_format_string("%i", current_version);
                append_string_builder(&node_desc_string_builder, to_append);
                append_string_builder(&node_desc_string_builder, ", ");
            };
            //Remove the last: ", "
            remove_last_string_builder(&node_desc_string_builder);
            append_string_builder(&node_desc_string_builder, ")");
            char * node_desc = to_string_string_builder(&node_desc_string_builder, NATIVE_LOX_ALLOCATOR());

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free_string_builder(&node_desc_string_builder);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            lox_value_t const_value = GET_CONST_VALUE_SSA_NODE(node);
            append_new_data_node_graphviz_file(visualizer, to_string(const_value), self_data_node_id);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, get_local->local_number);
            char * node_desc = dynamic_format_string("GetLocal %s", local_name);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * get_global = (struct ssa_data_get_global_node *) node;
            char * package_name = get_global->package->name;
            char * global_name = get_global->name->chars;
            char * node_desc = dynamic_format_string("GetGlobal\npackage: %s\nname: %s)", package_name, global_name);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, get_ssa_name->ssa_name.value.local_number);
            char * node_desc = dynamic_format_string("GetSSA %s %i\n", local_name, get_ssa_name->ssa_name.value.version);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
    }

    return self_data_node_id;
}

void link_block_block_node_graphviz_file(
        struct graphviz_visualizer * visualizer,
        struct block_graph_generated from,
        struct block_graph_generated to
) {
    link_control_control_node_graphviz_file(visualizer, from.value.last_control_node_id, to.value.first_control_node_id);
}

void link_data_data_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    char * link_node_text = dynamic_format_string("data_%i -> data_%i;", from, to);
    append_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void link_control_data_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    char * link_node_text = dynamic_format_string("control_%i -> data_%i;", from, to);
    append_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void link_control_control_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    char * link_node_text = dynamic_format_string("control_%i -> control_%i;", from, to);
    append_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void append_new_control_node_graphviz_file(struct graphviz_visualizer * visualizer, char * name, int control_node_id) {
    fprintf(visualizer->file, "\t\tcontrol_%i [label=%s, style=filled, fillcolor=yellow:white];\n", control_node_id, name);
}

void append_new_data_node_graphviz_file(struct graphviz_visualizer * visualizer, char * name, int data_node_id) {
    fprintf(visualizer->file, "\t\tdata_%i [label=%s, style=filled, fillcolor=green];\n", data_node_id, name);
}

void append_new_block_graphviz_file(struct graphviz_visualizer * visualizer, int block_id) {
    fprintf(visualizer->file, "subgraph block_%i{\n", block_id);
    fprintf(visualizer->file, "\tstyle=filled;\n");
    fprintf(visualizer->file, "\tcolor=lightgrey;\n");
}

void append_new_line_graphviz_file(struct graphviz_visualizer * visualizer, char * to_append) {
    fprintf(visualizer->file, to_append);
    fprintf(visualizer->file, "\n");
}

struct graphviz_visualizer create_graphviz_visualizer(
        char * path,
        struct arena_lox_allocator * arena_lox_allocator,
        struct function_object * function
) {
    FILE * file = fopen(path, "a");
    if (file == NULL) {
        exit(-1);
    }

    struct u64_hash_table last_block_control_node_by_block;
    init_u64_hash_table(&last_block_control_node_by_block, &arena_lox_allocator->lox_allocator);
    struct u64_set visited_blocks;
    init_u64_set(&visited_blocks, &arena_lox_allocator->lox_allocator);

    return (struct graphviz_visualizer) {
        .block_generated_graph_by_block = last_block_control_node_by_block,
        .visited_blocks = visited_blocks,
        .next_control_node_id = 0,
        .next_data_node_id = 0,
        .function = function,
        .next_block_id = 0,
        .file = file,
    };
}

static char * binary_operator_to_string(bytecode_t bytecode) {
    switch (bytecode) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_LESS: return "<";
        case OP_GREATER: return ">";
        case OP_EQUAL: return "==";
        default: exit(-1);
    }
}

static char * unary_operator_to_string(ssa_unary_operator_type_t operator) {
    switch (operator) {
        case UNARY_OPERATION_TYPE_NOT: return "not";
        case UNARY_OPERATION_TYPE_NEGATION: return "-";
    }
}