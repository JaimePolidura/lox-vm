#include "graphviz_visualizer.h"

extern void runtime_panic(char * format, ...);

struct graphviz_visualizer {
    FILE * file;

    long graphviz_options;
    long ssa_options;
    struct lox_ir ssa_ir;
    struct function_object * function;
    //Set of pointers of struct lox_ir_block visited
    struct u64_set blocks_graph_generated;
    //Last control control_node graph id by block pointer
    struct u64_hash_table block_generated_graph_by_block;
    //Stores struct edge_graph_generated
    struct u64_set blocks_edges_generated;

    int next_block_id;
    int next_control_node_id;
    int next_data_node_id;
};

struct edge_graph_generated {
    //We use a union so that we can store this struct in blocks_edges_generated as u64 easily
    union {
        struct {
            int from_block_last_control_node_id;
            int to_block_first_control_node_id;
        } value;
        uint64_t u64_value;
    };
};

struct block_graph_generated {
    //We use a union so that we can store this struct in block_generated_graph_by_block as u64 easily
    union {
        struct {
            int first_control_node_id;
            int last_control_node_id;
        } value;
        uint64_t u64_value;
    };
};

static void generate_graph_and_write(struct graphviz_visualizer *, struct lox_ir_block *);
static struct block_graph_generated generate_block_graph(struct graphviz_visualizer *, struct lox_ir_block *);
static int generate_control_node_graph(struct graphviz_visualizer *, struct lox_ir_control_node *);
static int generate_data_node_graph(struct graphviz_visualizer *, struct lox_ir_data_node *);
static struct block_graph_generated generate_blocks_graph(struct graphviz_visualizer *, struct lox_ir_block * first);
static char * maybe_add_type_info_data_node(struct graphviz_visualizer*, struct lox_ir_data_node*, char*);
static char * maybe_add_escape_info_data_node(struct graphviz_visualizer *visualizer, struct lox_ir_data_node *data_node, char *label);

static char * unary_operator_to_string(lox_ir_unary_operator_type_t);
static char * binary_operator_to_string(bytecode_t);
static char * guard_node_to_string(struct lox_ir_guard);

struct graphviz_visualizer create_graphviz_visualizer(char *, struct arena_lox_allocator *, struct function_object *);
void add_new_line_graphviz_file(struct graphviz_visualizer *, char * to_append);
void add_block_graphviz_file(struct graphviz_visualizer *, int);
void add_data_node_graphviz_file(struct graphviz_visualizer *, char *, int);
void add_control_node_graphviz_file(struct graphviz_visualizer *, char *, int);
void add_guard_control_node_graphviz_file(struct graphviz_visualizer *, struct lox_ir_control_guard_node *, int control_node_id);
void add_guard_data_node_graphviz_file(struct graphviz_visualizer *, struct lox_ir_data_guard_node *, int data_node_id);
void add_start_control_node_graphviz_file(struct graphviz_visualizer *);

void link_data_data_node_graphviz_file(struct graphviz_visualizer *, int from, int to);
void link_data_data_label_node_graphviz_file(struct graphviz_visualizer *, char *, int from, int to);
void link_control_data_node_graphviz_file(struct graphviz_visualizer *, int from, int to);
void link_control_data_node_label_graphviz_file(struct graphviz_visualizer *, char *, int from, int to);
void link_control_control_node_graphviz_file(struct graphviz_visualizer *, int from, int to);
void link_control_control_label_node_graphviz_file(struct graphviz_visualizer *, char *, int from, int to);
void link_block_block_label_node_graphviz_file(struct graphviz_visualizer *, char *, struct block_graph_generated, struct block_graph_generated);
void link_block_block_node_graphviz_file(struct graphviz_visualizer *, struct block_graph_generated, struct block_graph_generated);

void generate_ssa_graphviz_graph(
        struct package * package,
        struct function_object * function,
        phase_ssa_graphviz_t phase,
        long graphviz_options,
        long ssa_options,
        char * path
) {
    struct arena arena;
    init_arena(&arena);
    struct arena_lox_allocator ssa_node_allocator = to_lox_allocator_arena(arena);
    struct graphviz_visualizer graphviz_visualizer = create_graphviz_visualizer(path, &ssa_node_allocator, function);
    graphviz_visualizer.graphviz_options = graphviz_options;
    graphviz_visualizer.ssa_options = ssa_options;

    switch (phase) {
        case NO_PHIS_PHASE_LOX_IR_GRAPHVIZ: {
            struct lox_ir_block * ssa_block = create_ssa_ir_no_phis(package, function,
                                                                    create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator, ssa_options);

            generate_graph_and_write(&graphviz_visualizer, ssa_block);
            break;
        }
        case PHIS_INSERTED_PHASE_LOX_IR_GRAPHVIZ: {
            struct lox_ir_block * ssa_block = create_ssa_ir_no_phis(package, function,
                                                                    create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator, ssa_options);
            insert_ssa_ir_phis(ssa_block, &ssa_node_allocator);

            generate_graph_and_write(&graphviz_visualizer, ssa_block);
            break;
        }
        case PHIS_OPTIMIZED_PHASE_LOX_IR_GRAPHVIZ: {
            struct lox_ir_block * ssa_block = create_ssa_ir_no_phis(package, function,
                                                                    create_bytecode_list(function->chunk, &ssa_node_allocator.lox_allocator), &ssa_node_allocator, ssa_options);
            struct phi_insertion_result phi_insertion_result = insert_ssa_ir_phis(ssa_block, &ssa_node_allocator);
            optimize_ssa_ir_phis(ssa_block, &phi_insertion_result, &ssa_node_allocator);

            generate_graph_and_write(&graphviz_visualizer, ssa_block);
            break;
        }
        case SPARSE_CONSTANT_PROPAGATION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_sparse_constant_propagation(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case COMMON_SUBEXPRESSION_ELIMINATION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);

            perform_common_subexpression_elimination(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case STRENGTH_REDUCTION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_type_propagation(&ssa_ir);
            perform_strength_reduction(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case LOOP_INVARIANT_CODE_MOTION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_loop_invariant_code_motion(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case COPY_PROPAGATION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_copy_propagation(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case TYPE_PROPAGATION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_type_propagation(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case UNBOXING_INSERTION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_type_propagation(&ssa_ir);
            perform_unboxing_insertion(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case ESCAPE_ANALYSIS_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_type_propagation(&ssa_ir);
            perform_escape_analysis(&ssa_ir);
            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case PHI_RESOLUTION_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            graphviz_visualizer.ssa_ir = ssa_ir;

            resolve_phi(&ssa_ir);

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
        case ALL_PHASE_SSA_GRAPHVIZ: {
            struct lox_ir ssa_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                                                                                         &ssa_node_allocator.lox_allocator),
                                                 graphviz_visualizer.ssa_options);
            perform_sparse_constant_propagation(&ssa_ir);
            perform_common_subexpression_elimination(&ssa_ir);
            perform_loop_invariant_code_motion(&ssa_ir);
            perform_type_propagation(&ssa_ir);
            perform_strength_reduction(&ssa_ir);
            perform_copy_propagation(&ssa_ir);
            perform_unboxing_insertion(&ssa_ir);

            graphviz_visualizer.ssa_ir = ssa_ir;

            generate_graph_and_write(&graphviz_visualizer, ssa_ir.first_block);
            break;
        }
    }
}

static void generate_graph_and_write(struct graphviz_visualizer * graphviz_visualizer, struct lox_ir_block * first_block) {
    add_new_line_graphviz_file(graphviz_visualizer, "digraph G {");
    add_new_line_graphviz_file(graphviz_visualizer, "\trankdir=TB;");
    generate_blocks_graph(graphviz_visualizer, first_block);
    add_new_line_graphviz_file(graphviz_visualizer, "}");
}

static struct block_graph_generated generate_blocks_graph(struct graphviz_visualizer * visualizer, struct lox_ir_block * first_block) {
    struct block_graph_generated first_block_graph_data = generate_block_graph(visualizer, first_block);

    switch (first_block->type_next) {
        case TYPE_NEXT_LOX_IR_BLOCK_SEQ: {
            struct block_graph_generated next_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.next);
            link_block_block_node_graphviz_file(visualizer, first_block_graph_data, next_block_graph_data);
            break;
        }
        case TYPE_NEXT_LOX_IR_BLOCK_LOOP: {
            uint64_t loop_block_graph_data_u64 = (uint64_t) get_u64_hash_table(&visualizer->block_generated_graph_by_block,
                (uint64_t) first_block->next_as.loop);
            struct block_graph_generated loop_block_graph_data = (struct block_graph_generated){.u64_value = loop_block_graph_data_u64};
            link_block_block_label_node_graphviz_file(visualizer, "loop", first_block_graph_data, loop_block_graph_data);
            break;
        }
        case TYPE_NEXT_LOX_IR_BLOCK_BRANCH: {
            struct block_graph_generated branch_false_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.branch.false_branch);
            struct block_graph_generated branch_true_block_graph_data = generate_blocks_graph(visualizer, first_block->next_as.branch.true_branch);
            link_block_block_label_node_graphviz_file(visualizer, "false", first_block_graph_data, branch_false_block_graph_data);
            link_block_block_label_node_graphviz_file(visualizer, "true", first_block_graph_data, branch_true_block_graph_data);
            break;
        }
        case TYPE_NEXT_LOX_IR_BLOCK_NONE:
            break;
    }

    return first_block_graph_data;
}

static struct block_graph_generated generate_block_graph(struct graphviz_visualizer * visualizer, struct lox_ir_block * block) {
    if(contains_u64_set(&visualizer->blocks_graph_generated, (uint64_t) block)) {
        uint64_t block_generated_graph_by_block_u64 = (uint64_t) get_u64_hash_table(&visualizer->block_generated_graph_by_block, (uint64_t) block);
        return (struct block_graph_generated) {.u64_value = block_generated_graph_by_block_u64};
    }

    add_u64_set(&visualizer->blocks_graph_generated, (uint64_t) block);
    add_block_graphviz_file(visualizer, visualizer->next_block_id++);

    int first_control_node_id = 0;
    int last_control_node_id = 0;
    int prev_node_id = -1;

    //If we haven't added any control_node, add the start control_node
    if(visualizer->next_data_node_id == 0 && visualizer->next_control_node_id == 0){
        add_start_control_node_graphviz_file(visualizer);
        prev_node_id = visualizer->next_control_node_id - 1;
    }
    for(struct lox_ir_control_node * current = block->first;; current = current->next){
        int control_node_id = generate_control_node_graph(visualizer, current);

        if(current == block->first){
            first_control_node_id = control_node_id;
        }
        if(prev_node_id != -1){
            link_control_control_node_graphviz_file(visualizer, prev_node_id, control_node_id);
        }
        if(current == block->last){
            last_control_node_id = control_node_id;
            break;
        }

        prev_node_id = control_node_id;
    }

    add_new_line_graphviz_file(visualizer, "\t}");

    struct block_graph_generated block_graph_generated = (struct block_graph_generated) {
        .value = { first_control_node_id, last_control_node_id }
    };

    put_u64_hash_table(&visualizer->block_generated_graph_by_block, (uint64_t) block, (void *) block_graph_generated.u64_value);

    return block_graph_generated;
}

static int generate_control_node_graph(struct graphviz_visualizer * visualizer, struct lox_ir_control_node * node) {
    int self_control_node_id = visualizer->next_control_node_id++;
    switch (node->type) {
        case LOX_IR_CONTROL_NODE_SET_V_REGISTER: {
            struct lox_ir_control_set_v_register_node * set_v_reg = (struct lox_ir_control_set_v_register_node *) node;
            char * node_desc = dynamic_format_string("Set VRegister %i", set_v_reg->v_register);
            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int data_node_id = generate_data_node_graph(visualizer, set_v_reg->value);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_GUARD: {
            struct lox_ir_control_guard_node * guard = (struct lox_ir_control_guard_node *) node;
            add_guard_control_node_graphviz_file(visualizer, guard, self_control_node_id);
            break;
        }
        case LOX_IR_CONTROL_NODE_DATA: {
            struct lox_ir_control_data_node * data_control_node = (struct lox_ir_control_data_node *) node;

            add_control_node_graphviz_file(visualizer, "Data", self_control_node_id);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int data_node_id = generate_data_node_graph(visualizer, data_control_node->data);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_RETURN: {
            struct lox_ir_control_return_node * return_node = (struct lox_ir_control_return_node *) node;

            add_control_node_graphviz_file(visualizer, "Return", self_control_node_id);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int data_node_id = generate_data_node_graph(visualizer, return_node->data);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_PRINT: {
            struct lox_ir_control_print_node * print_node = (struct lox_ir_control_print_node *) node;

            add_control_node_graphviz_file(visualizer, "Print", self_control_node_id);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int data_node_id = generate_data_node_graph(visualizer, print_node->data);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_ENTER_MONITOR: {
            struct lox_ir_control_enter_monitor_node * enter_monitor = (struct lox_ir_control_enter_monitor_node *) node;
            char * node_desc = dynamic_format_string("EnterMonitor %i\\n0x", enter_monitor->monitor_number, enter_monitor->monitor);

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_CONTROL_NODE_EXIT_MONITOR: {
            struct lox_ir_control_exit_monitor_node * exit_monitor = (struct lox_ir_control_exit_monitor_node *) node;
            char * node_desc = dynamic_format_string("ExitMonitor %i\\n0x", exit_monitor->monitor_number, exit_monitor->monitor);

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_CONTORL_NODE_SET_GLOBAL: {
            struct lox_ir_control_set_global_node * set_global = (struct lox_ir_control_set_global_node *) node;
            char * package_name = set_global->package->name;
            char * global_name = set_global->name->chars;
            char * node_desc = dynamic_format_string("SetGlobal\\npackage: %s\\nname: %s", package_name, global_name);

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int global_value_data_node_id = generate_data_node_graph(visualizer, set_global->value_node);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, global_value_data_node_id);
            }
            break;
        }
        case LOX_IR_CONTORL_NODE_SET_LOCAL: {
            struct lox_ir_control_set_local_node * set_local = (struct lox_ir_control_set_local_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, set_local->local_number);
            char * node_desc = dynamic_format_string("SetLocal %s", local_name);

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int local_value_data_node_id = generate_data_node_graph(visualizer, set_local->new_local_value);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, local_value_data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_SET_STRUCT_FIELD: {
            struct lox_ir_control_set_struct_field_node * set_struct_field = (struct lox_ir_control_set_struct_field_node *) node;
            char * node_desc = dynamic_format_string("SetStructField %s", set_struct_field->field_name->chars);

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int field_value_data_node_id = generate_data_node_graph(visualizer, set_struct_field->new_field_value);
                int struct_instance_data_node_id = generate_data_node_graph(visualizer, set_struct_field->instance);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, field_value_data_node_id);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, struct_instance_data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_SET_ARRAY_ELEMENT: {
            struct lox_ir_control_set_array_element_node * set_array_element = (struct lox_ir_control_set_array_element_node *) node;
            char * node_desc = dynamic_format_string("SetArrayElement");

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int array_element_data_node_id = generate_data_node_graph(visualizer, set_array_element->new_element_value);
                int array_instance_data_node_id = generate_data_node_graph(visualizer, set_array_element->array);
                int array_index_data_node_id = generate_data_node_graph(visualizer, set_array_element->index);
                link_control_data_node_label_graphviz_file(visualizer, "newValue", self_control_node_id, array_element_data_node_id);
                link_control_data_node_label_graphviz_file(visualizer, "instance", self_control_node_id, array_instance_data_node_id);
                link_control_data_node_label_graphviz_file(visualizer, "index", self_control_node_id, array_index_data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_CONDITIONAL_JUMP: {
            struct lox_ir_control_conditional_jump_node * cond_jump = (struct lox_ir_control_conditional_jump_node *) node;
            char * node_desc = dynamic_format_string("ConditionalJump");
            
            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int condition_data_node_id = generate_data_node_graph(visualizer, cond_jump->condition);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, condition_data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_DEFINE_SSA_NAME: {
            struct lox_ir_control_define_ssa_name_node * define_ssa_name = (struct lox_ir_control_define_ssa_name_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, define_ssa_name->ssa_name.value.local_number);
            char * node_desc = dynamic_format_string("DefineSSA %s %i", local_name, define_ssa_name->ssa_name.value.version);

            add_control_node_graphviz_file(visualizer, node_desc, self_control_node_id);
            free(node_desc);
            long b = NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT;
            if(!IS_FLAG_SET(visualizer->graphviz_options, NOT_DISPLAY_DATA_NODES_GRAPHVIZ_OPT)) {
                int ssa_name_value_data_node_id = generate_data_node_graph(visualizer, define_ssa_name->value);
                link_control_data_node_graphviz_file(visualizer, self_control_node_id, ssa_name_value_data_node_id);
            }
            break;
        }
        case LOX_IR_CONTROL_NODE_LOOP_JUMP: {
            add_control_node_graphviz_file(visualizer, "Loop", self_control_node_id);
            break;
        }
    }

    return self_control_node_id;
}

static int generate_data_node_graph(struct graphviz_visualizer * visualizer, struct lox_ir_data_node * node) {
    int self_data_node_id = visualizer->next_data_node_id++;

    switch (node->type) {
        case LOX_IR_DATA_NODE_GUARD: {
            struct lox_ir_data_guard_node * guard_node = (struct lox_ir_data_guard_node *) node;
            int guard_value_node_id = generate_data_node_graph(visualizer, guard_node->guard.value);
            char * guard_node_desc = guard_node_to_string(guard_node->guard);
            char * node_desc = dynamic_format_string("Guard %s", guard_node_desc);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, guard_value_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_UNARY: {
            struct lox_ir_data_unary_node * unary = (struct lox_ir_data_unary_node *) node;
            int unary_value_node_id = generate_data_node_graph(visualizer, unary->operand);
            char * node_desc = dynamic_format_string("%s", unary_operator_to_string(unary->operator));
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, unary_value_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_BINARY: {
            struct lox_ir_data_binary_node * binary = (struct lox_ir_data_binary_node *) node;
            int left_node_id = generate_data_node_graph(visualizer, binary->left);
            int right_node_id = generate_data_node_graph(visualizer, binary->right);
            char * node_desc = dynamic_format_string("%s", binary_operator_to_string(binary->operator));
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, left_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, right_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_CALL: {
            struct lox_ir_data_function_call_node * call = (struct lox_ir_data_function_call_node *) node;
            char * function_name = call->is_native ? call->native_function->name : call->lox_function.function->name->chars;
            char * node_desc = dynamic_format_string("%s()", function_name);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            for (int i = 0; i < call->n_arguments; i++){
                int function_arg_node_id = generate_data_node_graph(visualizer, call->arguments[i]);
                link_data_data_node_graphviz_file(visualizer, self_data_node_id, function_arg_node_id);
            }
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_GET_STRUCT_FIELD: {
            struct lox_ir_data_get_struct_field_node * get_struct_field = (struct lox_ir_data_get_struct_field_node *) node;
            char * node_desc = dynamic_format_string("GetStructField %s", get_struct_field->field_name->chars);
            int struct_instance_node_id = generate_data_node_graph(visualizer, get_struct_field->instance_node);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);
            node_desc = maybe_add_escape_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, struct_instance_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_STRUCT: {
            struct lox_ir_data_initialize_struct_node * initialize_struct = (struct lox_ir_data_initialize_struct_node *) node;
            char * node_desc = dynamic_format_string("InitializeStruct %s", initialize_struct->definition->name);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);
            node_desc = maybe_add_escape_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            for(int i = 0; i < initialize_struct->definition->n_fields; i++){
                int struct_field_node_id = generate_data_node_graph(visualizer, initialize_struct->fields_nodes[i]);
                link_data_data_node_graphviz_file(visualizer, self_data_node_id, struct_field_node_id);
            }
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_LENGTH: {
            struct lox_ir_data_get_array_length * get_array_length = (struct lox_ir_data_get_array_length *) node;
            char * node_desc = maybe_add_type_info_data_node(visualizer, node, "GetArrayLength");

            add_data_node_graphviz_file(visualizer, "GetArrayLength", self_data_node_id);
            int array_instance_node_id = generate_data_node_graph(visualizer, get_array_length->instance);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, array_instance_node_id);
            break;
        }
        case LOX_IR_DATA_NODE_GET_ARRAY_ELEMENT: {
            struct lox_ir_data_get_array_element_node * get_array_element = (struct lox_ir_data_get_array_element_node *) node;
            char * node_desc = maybe_add_type_info_data_node(visualizer, node, "GetArrayElement");
            node_desc = maybe_add_escape_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            int array_instance_node_id = generate_data_node_graph(visualizer, get_array_element->instance);
            int array_index_node_id = generate_data_node_graph(visualizer, get_array_element->index);
            link_data_data_label_node_graphviz_file(visualizer, "intance", self_data_node_id, array_instance_node_id);
            link_data_data_label_node_graphviz_file(visualizer, "index", self_data_node_id, array_index_node_id);
            break;
        }
        case LOX_IR_DATA_NODE_UNBOX: {
            struct lox_ir_data_unbox_node * unbox = (struct lox_ir_data_unbox_node *) node;
            char * node_desc = dynamic_format_string("Unbox");
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            int unboxed_value_node_id = generate_data_node_graph(visualizer, unbox->to_unbox);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, unboxed_value_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_BOX: {
            struct lox_ir_data_box_node * box = (struct lox_ir_data_box_node *) node;
            char * node_desc = dynamic_format_string("Box");
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            int boxed_value_node_id = generate_data_node_graph(visualizer, box->to_box);
            link_data_data_node_graphviz_file(visualizer, self_data_node_id, boxed_value_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_INITIALIZE_ARRAY: {
            struct lox_ir_data_initialize_array_node *initialize_array = (struct lox_ir_data_initialize_array_node *) node;
            char * node_desc = dynamic_format_string("InitializeArray %i", initialize_array->n_elements);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);
            node_desc = maybe_add_escape_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            for (int i = 0; i < initialize_array->n_elements && !initialize_array->empty_initialization; i++) {
                int array_element_node_id = generate_data_node_graph(visualizer, initialize_array->elememnts_node[i]);
                link_data_data_node_graphviz_file(visualizer, self_data_node_id, array_element_node_id);
            }
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_PHI: {
            struct lox_ir_data_phi_node * phi = (struct lox_ir_data_phi_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, phi->local_number);
            struct string_builder node_desc_string_builder;
            init_string_builder(&node_desc_string_builder, NATIVE_LOX_ALLOCATOR());
            append_string_builder(&node_desc_string_builder, "Phi ");
            append_string_builder(&node_desc_string_builder, local_name);
            append_string_builder(&node_desc_string_builder, "(");
            FOR_EACH_SSA_NAME_IN_PHI_NODE(phi, current_version) {
                char * to_append = dynamic_format_string("%i", current_version.value.version);
                append_string_builder(&node_desc_string_builder, to_append);
                append_string_builder(&node_desc_string_builder, ", ");
            };

            //Remove the last: ", "
            remove_last_string_builder(&node_desc_string_builder);
            append_string_builder(&node_desc_string_builder, ")");
            char * node_desc = to_string_string_builder(&node_desc_string_builder, NATIVE_LOX_ALLOCATOR());
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free_string_builder(&node_desc_string_builder);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_CONSTANT: {
            lox_value_t const_value = GET_CONST_VALUE_LOX_IR_NODE(node);
            char * node_desc = maybe_add_type_info_data_node(visualizer, node, lox_value_to_string(const_value));

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            break;
        }
        case LOX_IR_DATA_NODE_GET_LOCAL: {
            struct lox_ir_data_get_local_node * get_local = (struct lox_ir_data_get_local_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, get_local->local_number);
            char * node_desc = dynamic_format_string("GetLocal %s", local_name);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_GET_GLOBAL: {
            struct lox_ir_data_get_global_node * get_global = (struct lox_ir_data_get_global_node *) node;
            char * package_name = get_global->package->name;
            char * global_name = get_global->name->chars;
            char * node_desc = dynamic_format_string("GetGlobal\\npackage: %s\\nname: %s)", package_name, global_name);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_GET_SSA_NAME: {
            struct lox_ir_data_get_ssa_name_node * get_ssa_name = (struct lox_ir_data_get_ssa_name_node *) node;
            char * local_name = get_u8_hash_table(&visualizer->function->local_numbers_to_names, get_ssa_name->ssa_name.value.local_number);
            char * node_desc = dynamic_format_string("GetSSA %s %i", local_name, get_ssa_name->ssa_name.value.version);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
        case LOX_IR_DATA_NODE_GET_V_REGISTER: {
            struct lox_ir_data_get_v_register_node * get_v_reg = (struct lox_ir_data_get_v_register_node *) node;
            char * node_desc = dynamic_format_string("GetVRegister %i", get_v_reg->v_register);
            node_desc = maybe_add_type_info_data_node(visualizer, node, node_desc);

            add_data_node_graphviz_file(visualizer, node_desc, self_data_node_id);
            free(node_desc);
            break;
        }
    }

    return self_data_node_id;
}

void link_block_block_label_node_graphviz_file(
        struct graphviz_visualizer * visualizer,
        char * label,
        struct block_graph_generated from,
        struct block_graph_generated to
) {
    link_control_control_label_node_graphviz_file(visualizer, label, from.value.last_control_node_id, to.value.first_control_node_id);
}

void link_block_block_node_graphviz_file(
        struct graphviz_visualizer * visualizer,
        struct block_graph_generated from,
        struct block_graph_generated to
) {
    link_control_control_node_graphviz_file(visualizer, from.value.last_control_node_id, to.value.first_control_node_id);
}

void link_data_data_label_node_graphviz_file(struct graphviz_visualizer * visualizer, char * label, int from, int to) {
    char * link_node_text = dynamic_format_string("\t\tdata_%i -> data_%i [label=\"%s\"];", from, to, label);
    add_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void link_data_data_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    char * link_node_text = dynamic_format_string("\t\tdata_%i -> data_%i;", from, to);
    add_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void link_control_data_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    char * link_node_text = dynamic_format_string("\t\tcontrol_%i -> data_%i;", from, to);
    add_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void link_control_data_node_label_graphviz_file(struct graphviz_visualizer * visualizer, char * label, int from, int to) {
    char * link_node_text = dynamic_format_string("\t\tcontrol_%i -> data_%i [label=\"%s\"];", from, to, label);
    add_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
    free(link_node_text);
}

void link_control_control_label_node_graphviz_file(struct graphviz_visualizer * visualizer, char * label, int from, int to) {
    struct edge_graph_generated edge = (struct edge_graph_generated) {.value = {from, to}};

    if(!contains_u64_set(&visualizer->blocks_edges_generated, edge.u64_value)){
        char * link_node_text = dynamic_format_string("\t\tcontrol_%i -> control_%i [penwidth=3, label=\"%s\"];", from, to, label);
        add_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
        free(link_node_text);
        add_u64_set(&visualizer->blocks_edges_generated, edge.u64_value);
    }
}

void link_control_control_node_graphviz_file(struct graphviz_visualizer * visualizer, int from, int to) {
    struct edge_graph_generated edge = (struct edge_graph_generated) {.value = {from, to}};

    if(!contains_u64_set(&visualizer->blocks_edges_generated, edge.u64_value)){
        char * link_node_text = dynamic_format_string("\t\tcontrol_%i -> control_%i [penwidth=3];", from, to);
        add_new_line_graphviz_file(visualizer, dynamic_format_string(link_node_text));
        free(link_node_text);
        add_u64_set(&visualizer->blocks_edges_generated, edge.u64_value);
    }
}

void add_start_control_node_graphviz_file(struct graphviz_visualizer * visualizer) {
    int start_control_node_id = visualizer->next_control_node_id++;
    fprintf(visualizer->file, "\t\tcontrol_%i [label=\"START\", style=filled, fillcolor=blue, shape=rectangle];\n", start_control_node_id);
}

void add_guard_control_node_graphviz_file(struct graphviz_visualizer * visualizer, struct lox_ir_control_guard_node * guard_node, int control_node_id) {
    char * guard_desc = guard_node_to_string(guard_node->guard);
    char * node_desc = dynamic_format_string("Guard %s\n", guard_desc);
    fprintf(visualizer->file, "\t\tcontrol_%i [label=\"%s\", style=filled, fillcolor=orange, shape=rectangle];\n", control_node_id, node_desc);
    free(node_desc);
    free(guard_desc);
}

void add_guard_data_node_graphviz_file(struct graphviz_visualizer * visualizer, struct lox_ir_data_guard_node * guard_node, int data_node_id) {
    char * guard_desc = guard_node_to_string(guard_node->guard);
    char * node_desc = dynamic_format_string("Guard %s\n", guard_desc);
    fprintf(visualizer->file, "\t\tdata_%i [label=\"%s\", style=filled, fillcolor=orange];\n", data_node_id, node_desc);
    free(node_desc);
    free(guard_desc);
}

void add_control_node_graphviz_file(struct graphviz_visualizer * visualizer, char * name, int control_node_id) {
    fprintf(visualizer->file, "\t\tcontrol_%i [label=\"%s\", style=filled, fillcolor=yellow, shape=rectangle];\n", control_node_id, name);
}

void add_data_node_graphviz_file(struct graphviz_visualizer * visualizer, char * name, int data_node_id) {
    fprintf(visualizer->file, "\t\tdata_%i [label=\"%s\", style=filled, fillcolor=green];\n", data_node_id, name);
}

void add_block_graphviz_file(struct graphviz_visualizer * visualizer, int block_id) {
    if((visualizer->graphviz_options & NOT_DISPLAY_BLOCKS_GRAPHVIZ_OPT) != 0){
        fprintf(visualizer->file, "\tsubgraph block_%i{\n", block_id);
    } else {
        fprintf(visualizer->file, "\tsubgraph cluster_block_%i{\n", block_id);
    }

    fprintf(visualizer->file, "\t\tstyle=filled;\n");
    fprintf(visualizer->file, "\t\tcolor=lightgrey;\n");
}

void add_new_line_graphviz_file(struct graphviz_visualizer * visualizer, char * to_append) {
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
        runtime_panic("Cannot open/create file!");
        exit(-1);
    }

    struct u64_hash_table last_block_control_node_by_block;
    init_u64_hash_table(&last_block_control_node_by_block, &arena_lox_allocator->lox_allocator);
    struct u64_set visited_blocks;
    init_u64_set(&visited_blocks, &arena_lox_allocator->lox_allocator);
    struct u64_set blocks_edges_generated;
    init_u64_set(&blocks_edges_generated, &arena_lox_allocator->lox_allocator);

    return (struct graphviz_visualizer) {
        .block_generated_graph_by_block = last_block_control_node_by_block,
        .blocks_edges_generated = blocks_edges_generated,
        .blocks_graph_generated = visited_blocks,
        .next_control_node_id = 0,
        .next_data_node_id = 0,
        .function = function,
        .next_block_id = 0,
        .file = file,
    };
}

static char * binary_operator_to_string(bytecode_t bytecode) {
    switch (bytecode) {
        case OP_BINARY_OP_AND: return "&";
        case OP_BINARY_OP_OR: return "|";
        case OP_RIGHT_SHIFT: return ">>";
        case OP_LEFT_SHIFT: return "<<";
        case OP_GREATER: return ">";
        case OP_EQUAL: return "==";
        case OP_MODULO: return "%";
        case OP_LESS: return "<";
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        default: exit(-1);
    }
}

static char * unary_operator_to_string(lox_ir_unary_operator_type_t operator) {
    switch (operator) {
        case UNARY_OPERATION_TYPE_NOT: return "not";
        case UNARY_OPERATION_TYPE_NEGATION: return "-";
    }
}

static char * guard_node_to_string(struct lox_ir_guard guard) {
    char * guard_desc = NULL;

    switch (guard.type) {
        case LOX_IR_GUARD_ARRAY_TYPE_CHECK: {
            char * type_name = to_string_lox_ir_type(guard.value_to_compare.type);
            guard_desc = dynamic_format_string("ArrayTypeCheck %s", type_name);
            break;
        }
        case LOX_IR_GUARD_TYPE_CHECK: {
            char * type_name = to_string_lox_ir_type(guard.value_to_compare.type);
            guard_desc = dynamic_format_string("TypeCheck %s", type_name);
            break;
        }
        case LOX_IR_GUARD_BOOLEAN_CHECK: {
            char * value_name = lox_value_to_string(AS_BOOL(guard.value_to_compare.check_true));
            guard_desc = dynamic_format_string("BooleanValueCheck %s", value_name);
            break;
        }
        case LOX_IR_GUARD_STRUCT_DEFINITION_TYPE_CHECK: {
            struct struct_definition_object * definition = (struct struct_definition_object *)
                    guard.value_to_compare.struct_definition;
            guard_desc = dynamic_format_string("StructDefinitionCheck %s", definition != NULL ? definition->name->chars : "Any definition");
            break;
        }
    }

    char * guard_fail_action_type = guard.action_on_guard_failed == LOX_IR_GUARD_FAIL_ACTION_TYPE_SWITCH_TO_INTERPRETER ?
            "SwitchToInterpreter" : "RuntimeError";
    guard_desc = dynamic_format_string("%s\nOnFail: %s", guard_desc, guard_fail_action_type);
    return guard_desc;
}

static char * maybe_add_escape_info_data_node(
        struct graphviz_visualizer * visualizer,
        struct lox_ir_data_node * data_node,
        char * label
) {
    if(IS_FLAG_SET(visualizer->graphviz_options, DISPLAY_ESCAPE_INFO_OPT) && data_node->produced_type != NULL) {
        bool escapes = is_marked_as_escaped_lox_ir_data_node(data_node);
        return dynamic_format_string("%s\nEscapes: %s", label, escapes ? "true" : "false");
    } else {
        return label;
    }
}

static char * maybe_add_type_info_data_node(
        struct graphviz_visualizer * visualizer,
        struct lox_ir_data_node * data_node,
        char * label
) {
    if(IS_FLAG_SET(visualizer->graphviz_options, DISPLAY_TYPE_INFO_OPT) && data_node->produced_type != NULL){
        return dynamic_format_string("%s\nType: %s", label, to_string_lox_ir_type(data_node->produced_type->type));
    } else {
        return label;
    }
}