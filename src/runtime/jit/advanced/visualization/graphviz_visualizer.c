#include "graphviz_visualizer.h"

struct graphviz_visualizer {
    FILE * file;

    struct ssa_ir ssa_ir;
    struct function_object * function;
    //Set of block pointers visualized
    struct u64_set blocks_visisted;
    int next_block_id;
    int next_control_node_id;
    int next_data_node_id;
};

static void generate_graph_and_write(struct graphviz_visualizer *, struct ssa_block *);
static void generate_block_graph(struct graphviz_visualizer *, struct ssa_block *);
static int generate_control_node_graph(struct graphviz_visualizer *, struct ssa_control_node *);
static int generate_data_node_graph(struct graphviz_visualizer *, struct ssa_data_node *);

static char * unary_operator_to_string(ssa_unary_operator_type_t);
static char * binary_operator_to_string(bytecode_t);

struct graphviz_visualizer create_graphviz_visualizer(char *, struct arena_lox_allocator *, struct function_object *);
void append_new_line_graphviz_file(struct graphviz_visualizer *, char * to_append);
void append_new_block_graphviz_file(struct graphviz_visualizer *, int);
void append_new_data_node_graphviz_file(struct graphviz_visualizer *, char *, int);
void link_data_data_node_graphviz_file(struct graphviz_visualizer *visualizer, int from, int to);

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

    generate_block_graph(graphviz_visualizer, first_block);

    append_new_line_graphviz_file(graphviz_visualizer, "}");
}

static void generate_block_graph(struct graphviz_visualizer * graphviz_visualizer, struct ssa_block * block) {
    if(contains_u64_set(&graphviz_visualizer->blocks_visisted, (uint64_t) block)){
        return;
    }
    add_u64_set(&graphviz_visualizer->blocks_visisted, (uint64_t) block);
    int new_block_id = graphviz_visualizer->next_block_id++;

    append_new_block_graphviz_file(graphviz_visualizer, new_block_id);

    for(struct ssa_control_node * current = block->first;; current = current->next){
        generate_control_node_graph(graphviz_visualizer, current);

        if(current == block->last){
            break;
        }
    }

    append_new_line_graphviz_file(graphviz_visualizer, "}");
}

static int generate_control_node_graph(struct graphviz_visualizer * visualizer, struct ssa_control_node * node) {
    int new_node_id = visualizer->next_control_node_id++;
    switch (node->type) {
        case SSA_CONTROL_NODE_TYPE_DATA:
            struct ssa_data_node * data = (struct ssa_data_node *) node;
            generate_data_node_graph(visualizer, data);
            break;
        case SSA_CONTROL_NODE_TYPE_START:
            break;
        case SSA_CONTROL_NODE_TYPE_RETURN:
            break;
        case SSA_CONTROL_NODE_TYPE_PRINT:
            break;
        case SSA_CONTROL_NODE_TYPE_ENTER_MONITOR:
            break;
        case SSA_CONTROL_NODE_TYPE_EXIT_MONITOR:
            break;
        case SSA_CONTORL_NODE_TYPE_SET_GLOBAL:
            break;
        case SSA_CONTORL_NODE_TYPE_SET_LOCAL:
            break;
        case SSA_CONTROL_NODE_TYPE_SET_STRUCT_FIELD:
            break;
        case SSA_CONTROL_NODE_TYPE_SET_ARRAY_ELEMENT:
            break;
        case SSA_CONTROL_NODE_TYPE_LOOP_JUMP:
            break;
        case SSA_CONTROL_NODE_TYPE_CONDITIONAL_JUMP:
            break;
        case SSA_CONTROL_NODE_TYPE_DEFINE_SSA_NAME:
            break;
    }

    return new_node_id;
}

static int generate_data_node_graph(struct graphviz_visualizer * visualizer, struct ssa_data_node * node) {
    int self_node_id = visualizer->next_data_node_id++;

    switch (node->type) {
        case SSA_DATA_NODE_TYPE_UNARY: {
            struct ssa_data_unary_node * unary = (struct ssa_data_unary_node *) node;
            int unary_value_node_id = generate_data_node_graph(visualizer, unary->operand);
            char * node_desc = dynamic_format_string("%s", unary_operator_to_string(unary->operator_type));

            append_new_data_node_graphviz_file(visualizer, node_desc, self_node_id);
            link_data_data_node_graphviz_file(visualizer, self_node_id, unary_value_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_BINARY: {
            struct ssa_data_binary_node * binary = (struct ssa_data_binary_node *) node;
            int left_node_id = generate_data_node_graph(visualizer, binary->left);
            int right_node_id = generate_data_node_graph(visualizer, binary->right);
            char * node_desc = dynamic_format_string("%s", binary_operator_to_string(binary->operand));
            append_new_data_node_graphviz_file(visualizer, node_desc, self_node_id);

            link_data_data_node_graphviz_file(visualizer, self_node_id, left_node_id);
            link_data_data_node_graphviz_file(visualizer, self_node_id, right_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_CALL: {
            struct ssa_data_function_call_node * call = (struct ssa_data_function_call_node *) node;
            int function_node_id = generate_data_node_graph(visualizer, call->function);

            append_new_data_node_graphviz_file(visualizer, "Call", self_node_id);
            link_data_data_node_graphviz_file(visualizer, self_node_id, function_node_id);
            for (int i = 0; i < call->n_arguments; i++){
                int function_arg_node_id = generate_data_node_graph(visualizer, call->arguments[i]);
                link_data_data_node_graphviz_file(visualizer, self_node_id, function_arg_node_id);
            }
            break;
        }
        case SSA_DATA_NODE_TYPE_PHI: {
            struct ssa_data_phi_node * phi = (struct ssa_data_phi_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, phi->local_number);
            char * node_desc = dynamic_format_string("Phi %s (", local_name);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_node_id);
            FOR_EACH_VERSION_IN_PHI_NODE(phi, current_version) {
            };

            break;
        }
        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD: {
            struct ssa_data_get_struct_field_node * get_struct_field = (struct ssa_data_get_struct_field_node *) node;
            char * node_desc = dynamic_format_string("GetStructField %s (", get_struct_field->field_name->chars);
            break;
        }
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
            break;
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
            break;
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
            break;
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            lox_value_t const_value = GET_CONST_VALUE_SSA_NODE(node);
            append_new_data_node_graphviz_file(visualizer, to_string(const_value), self_node_id);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, get_local->local_number);
            char * node_desc = dynamic_format_string("GetLocal %s", local_name);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * get_global = (struct ssa_data_get_global_node *) node;
            char * package_name = get_global->package->name;
            char * global_name = get_global->name->chars;
            char * node_desc = dynamic_format_string("GetGlobal\npackage: %s\nname: %s)", package_name, global_name);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_node_id);
            free(node_desc);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME: {
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, get_ssa_name->ssa_name.value.local_number);
            char * node_desc = dynamic_format_string("SSA Name %s %i\n", local_name, get_ssa_name->ssa_name.value.version);

            append_new_data_node_graphviz_file(visualizer, node_desc, self_node_id);
            free(node_desc);
            break;
        }
    }

    return self_node_id;
}

void link_data_data_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    char * link_node_text = dynamic_format_string("data_%i -> data_%i;", from, to);
    append_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
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

    struct u64_set blocks_visited;
    init_u64_set(&blocks_visited, &arena_lox_allocator->lox_allocator);

    return (struct graphviz_visualizer) {
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