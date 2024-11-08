#pragma once

#include "runtime/jit/advanced/ssa_creator.h"
#include "compiler/compiler.h"
#include "test.h"

TEST(simple_ssa_ir_test){
    struct compilation_result compilation = compile_standalone(
            "fun function_ssa(a, b, c) {"
            "   if(a > 0) {"
            "       if(b > 0) {"
            "           print 1;"
            "       } else {"
            "           print 2;"
            "       }"
            "   } else {"
            "       for(var i = 0; i < 10; i = i + 1){"
            "           print 3;"
            "       }"
            "       print 4;"
            "   }"
            "   print 5;"
            "}"
    );

    struct package * package = compilation.compiled_package;
    struct function_object * function_ssa = get_function_package(package, "function_ssa");
    int n_instructions = function_ssa->chunk->in_use;
    init_function_profile_data(&function_ssa->state_as.profiling.profile_data, n_instructions, function_ssa->n_locals);

    struct ssa_control_node * start_ssa_ir = create_ssa_ir(
            package, function_ssa, create_bytecode_list(function_ssa->chunk)
    );
}