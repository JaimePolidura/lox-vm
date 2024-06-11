#pragma once

#include "test.h"
#include "runtime/vm.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/bytecode/chunk/chunk_disassembler.h"
#include "compiler/compiler.h"

extern struct vm current_vm;

TEST(vm_inline_test_simple_function){
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            "var a = inline suma(1, 2);"
            "var b = inline suma(3, 3);"
            "print a + b;"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();
    
    ASSERT_NEXT_VM_LOG(current_vm, "9.000000");
}