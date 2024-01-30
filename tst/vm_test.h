#pragma once

#include "test.h"
#include "../src/vm/vm.h"
#include "../src/compiler/compiler.h"

extern struct vm current_vm;

TEST(simple_vm_test_with_functions) {
    struct compilation_result compilation_result = compile("fun sumar(a, b) {\nvar c = a + b;\nreturn c;\n}\nprint sumar(1, 2);");
    start_vm();

    interpret_result vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
}

TEST(simple_vm_test_with_scope_variables) {
    struct compilation_result compilation_result = compile("var edad = 10;\n{\nvar nombre = 1;\nnombre = 1.0;\nprint nombre;}");
    start_vm();
    interpret_result vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == EXIT_SUCCESS);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
}