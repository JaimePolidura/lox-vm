#pragma once

#include "test.h"
#include "../src/vm/vm.h"
#include "../src/compiler/compiler.h"


TEST(simple_vm_test_with_scope_variables) {
    struct compilation_result compilation_result = compile("var edad = 10;\n{\nvar nombre = \"jaime\";\nnombre = 1;\nprint nombre;}");
    start_vm();
    interpret_result vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == EXIT_SUCCESS);
}