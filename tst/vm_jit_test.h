#pragma once

#include "test.h"
#include "runtime/vm.h"
#include "compiler/compiler.h"

extern struct vm current_vm;

TEST(vm_jit_simple_function_test) {
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun sumar(a, b) {"
            "   var c = a + b;"
            "   return c;"
            "}"
            "forceJIT(sumar);"
            ""
            "print sumar(1, 3);"
            );

    interpret_vm(compilation);

    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "4.000000");
}