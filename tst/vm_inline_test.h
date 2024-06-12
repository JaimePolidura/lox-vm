#pragma once

#include "test.h"
#include "runtime/vm.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/bytecode/chunk/chunk_disassembler.h"
#include "compiler/compiler.h"

extern struct vm current_vm;

TEST(vm_inline_multiple_returns_test){
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun numero(a) {"
            "   if (a > 100) {"
            "       return 1;"
            "   } else {"
            "       return 2;"
            "   }"
            "}"
            ""
            "print inline numero(101);"
            "print inline numero(99);"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
}

TEST(vm_inline_monitor_test){
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun sync nada() {"
            "   sync {"
            "       print 1;"
            "   }"
            "   sync {"
            "       print 2;"
            "   }"
            "}"
            ""
            "inline nada();"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
}

TEST(vm_inline_if_test) {
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun nada() {"
            "   var c = 1 + 2;"
            "   if(c == 3) {"
            "       print c;"
            "   }"
            "}"
            ""
            "if (true) {"
            "   inline nada();"
            "}"
            "print 1;"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
}

TEST(vm_inline_for_loop_test) {
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            "for(var i = 0; i < 5; i = i + 1) {"
            "   print inline suma(i, i);"
            "}"
            "print 1;"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "0.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "4.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "6.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "8.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
}

TEST(vm_inline_simple_function_test) {
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