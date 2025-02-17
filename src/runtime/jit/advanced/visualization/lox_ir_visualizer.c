#include "lox_ir_visualizer.h"

extern void runtime_panic(char * format, ...);

static struct lox_ir_visualizer create_graphviz_visualizer(char *, struct arena_lox_allocator *, struct function_object *);

void visualize_lox_ir(
        struct package * package,
        struct function_object * function,
        phase_lox_ir_lox_ir_visualizer_t phase,
        long graphviz_options,
        long options,
        char * path
) {
    struct arena arena;
    init_arena(&arena);
    struct arena_lox_allocator node_allocator = to_lox_allocator_arena(arena);
    struct lox_ir_visualizer graphviz_visualizer = create_graphviz_visualizer(path, &node_allocator, function);
    graphviz_visualizer.graphviz_options = graphviz_options;
    graphviz_visualizer.options = options;

    switch (phase) {
        case NO_PHIS_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir_block * block = create_lox_ir_no_phis(package, function,
                    create_bytecode_list(function->chunk, &node_allocator.lox_allocator), &node_allocator, options);

            generate_graph_visualization_lox_ir(&graphviz_visualizer, block);
            break;
        }
        case PHIS_INSERTED_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir_block * block = create_lox_ir_no_phis(package, function,
                    create_bytecode_list(function->chunk, &node_allocator.lox_allocator), &node_allocator, options);
            insert_lox_ir_phis(block, &node_allocator);

            generate_graph_visualization_lox_ir(&graphviz_visualizer, block);
            break;
        }
        case PHIS_OPTIMIZED_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir_block * block = create_lox_ir_no_phis(package, function,
                    create_bytecode_list(function->chunk, &node_allocator.lox_allocator), &node_allocator, options);
            struct phi_insertion_result phi_insertion_result = insert_lox_ir_phis(block, &node_allocator);
            optimize_lox_ir_phis(block, &phi_insertion_result, &node_allocator);

            generate_graph_visualization_lox_ir(&graphviz_visualizer, block);
            break;
        }
        case SPARSE_CONSTANT_PROPAGATION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_sparse_constant_propagation(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case COMMON_SUBEXPRESSION_ELIMINATION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);

            perform_common_subexpression_elimination(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case STRENGTH_REDUCTION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_type_propagation(&lox_ir);
            perform_strength_reduction(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case LOOP_INVARIANT_CODE_MOTION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_loop_invariant_code_motion(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case COPY_PROPAGATION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_copy_propagation(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case TYPE_PROPAGATION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_type_propagation(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case UNBOXING_INSERTION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_type_propagation(&lox_ir);
            perform_unboxing_insertion(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case ESCAPE_ANALYSIS_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_type_propagation(&lox_ir);
            perform_escape_analysis(&lox_ir);
            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case PHI_RESOLUTION_PHASE_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            graphviz_visualizer.lox_ir = lox_ir;
            perform_type_propagation(&lox_ir);
            perform_unboxing_insertion(&lox_ir);
            perform_copy_propagation(&lox_ir);
            resolve_phi(&lox_ir);

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case LOWERING_LOX_IR_VISUALIZATION: {
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            perform_type_propagation(&lox_ir);
            perform_unboxing_insertion(&lox_ir);
            //perform_copy_propagation(&lox_ir);
            resolve_phi(&lox_ir);
            lower_lox_ir(&lox_ir);

            graphviz_visualizer.lox_ir = lox_ir;
            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
        case ALL_PHASE_LOX_IR_VISUALIZATION: {
            //IR Creation
            struct lox_ir lox_ir = create_lox_ir(package, function, create_bytecode_list(function->chunk,
                    &node_allocator.lox_allocator), graphviz_visualizer.options);
            //Optimizations
            perform_sparse_constant_propagation(&lox_ir);
            perform_loop_invariant_code_motion(&lox_ir);
            perform_type_propagation(&lox_ir);
            perform_strength_reduction(&lox_ir);
            perform_unboxing_insertion(&lox_ir);
            perform_common_subexpression_elimination(&lox_ir);
            perform_copy_propagation(&lox_ir);
            //Code emition
            resolve_phi(&lox_ir);
            lower_lox_ir(&lox_ir);

            graphviz_visualizer.lox_ir = lox_ir;

            generate_graph_visualization_lox_ir(&graphviz_visualizer, lox_ir.first_block);
            break;
        }
    }
}

static struct lox_ir_visualizer create_graphviz_visualizer(
        char * path,
        struct arena_lox_allocator * arena_lox_allocator,
        struct function_object * function
) {
    FILE * file = fopen(path, "wa");
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

    return (struct lox_ir_visualizer) {
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