#include "graphviz_visualizer.h"

struct graphviz_visualizer {
    FILE * file;

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

struct graphviz_visualizer create_graphviz_visualizer(char *, struct arena_lox_allocator *);
void append_new_line_graphviz_file(struct graphviz_visualizer *, char * to_append);
void append_new_block_graphviz_file(struct graphviz_visualizer *, int);
void append_new_data_node_graphviz_file(struct graphviz_visualizer *, char *, int);

void generate_ssa_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_ssa_graphviz_t phase,
        char * path
) {
    struct arena arena;
    init_arena(&arena);
    struct arena_lox_allocator ssa_node_allocator = to_lox_allocator_arena(arena);
    struct graphviz_visualizer graphviz_visualizer = create_graphviz_visualizer(path, &ssa_node_allocator);


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

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case ALL_PHASE_SSA_GRAPHVIZ: {
            struct ssa_ir ssa_ir = create_ssa_ir(package, function, create_bytecode_list(function->chunk,
                &ssa_node_allocator.lox_allocator));
            perform_sparse_constant_propagation(&ssa_ir);

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
    }
}

static void generate_graph_and_write(struct graphviz_visualizer * graphviz_visualizer, struct ssa_block * start_block) {
    append_new_line_graphviz_file(graphviz_visualizer, "digraph G {");

    generate_block_graph(graphviz_visualizer, start_block);

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
    int new_node_id = visualizer->next_data_node_id++;

    switch (node->type) {
        case SSA_DATA_NODE_TYPE_CONSTANT: {
            lox_value_t const_value = GET_CONST_VALUE_SSA_NODE(node);
            append_new_data_node_graphviz_file(visualizer, to_string(const_value), new_node_id);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_LOCAL: {
            struct ssa_data_get_local_node * get_local = (struct ssa_data_get_local_node *) node;
            char node_name[16];
            sprintf(node_name, "GET_LOCAL %i", get_local->local_number);
            append_new_data_node_graphviz_file(visualizer, node_name, new_node_id);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_GLOBAL: {
            struct ssa_data_get_global_node * get_global = (struct ssa_data_get_global_node *) node;
            char node_name[32 + strlen(get_global->package->name) + get_global->name->length];
            sprintf(node_name, "GET_GLOBAL\npackage: %s\nname: %s)", get_global->package->name, get_global->name->chars);
            append_new_data_node_graphviz_file(visualizer, node_name, new_node_id);
            break;
        }
        case SSA_DATA_NODE_TYPE_GET_SSA_NAME:
            struct ssa_data_get_ssa_name_node * get_ssa_name = (struct ssa_data_get_ssa_name_node *) node;
            char node_name[32];
            break;

        case SSA_DATA_NODE_TYPE_CALL:
            break;
        case SSA_DATA_NODE_TYPE_BINARY:
            break;
        case SSA_DATA_NODE_TYPE_UNARY:
            break;

        case SSA_DATA_NODE_TYPE_GET_STRUCT_FIELD:
            break;
        case SSA_DATA_NODE_TYPE_INITIALIZE_STRUCT:
            break;
        case SSA_DATA_NODE_TYPE_GET_ARRAY_ELEMENT:
            break;
        case SSA_DATA_NODE_TYPE_INITIALIZE_ARRAY:
            break;
        case SSA_DATA_NODE_TYPE_PHI:
            break;

    }

    return new_node_id;
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


struct graphviz_visualizer create_graphviz_visualizer(char * path, struct arena_lox_allocator * arena_lox_allocator) {
    FILE * file = fopen(path, "a");
    if (file == NULL) {
        exit(-1);
    }

    struct u64_set blocks_visited;
    init_u64_set(&blocks_visited, &arena_lox_allocator->lox_allocator);

    return (struct graphviz_visualizer) {
            .next_control_node_id = 0,
            .next_data_node_id = 0,
            .next_block_id = 0,
            .file = file,
    };
}