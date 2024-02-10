#pragma once

#include "test.h"
#include "../src/vm/vm.h"
#include "compiler/compiler.h"

extern struct vm current_vm;

TEST(vm_file_global_variables_test) {
    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\global_variables\\main.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\global_variables",
            "main"
    );
    start_vm();
    
    interpret_result_t vm_result = interpret_vm(result);
    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");

    stop_vm();
}

TEST(simple_vm_test_with_structs){
    struct compilation_result compilation_result = compile_standalone(
            "struct Persona{ nombre; edad; } var jaime = Persona{\"Jaime\", 21}; print jaime.nombre; jaime.edad = 21 + 1; print jaime.edad;"
    );
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "Jaime");
    ASSERT_NEXT_VM_LOG(current_vm, "22.000000");

    stop_vm();
}

TEST(simple_vm_test_with_while) {
    struct compilation_result compilation_result = compile_standalone("var i = 0; while(i < 2) { print i; i = i + 1; }");
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "0.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");

    stop_vm();
}

TEST(simple_vm_test_with_ifs) {
    struct compilation_result compilation_result = compile_standalone(
            "if(1 == 1) {\n print 1;\n }\n if(1 == 2) {\n print 2;\n } else {\n print 3;\n } if (1 == 2) { print 4; } print 5;");
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "5.000000");

    stop_vm();
}

TEST(simple_vm_test_with_for_loops) {
    struct compilation_result compilation_result = compile_standalone(
            "for(var i = 0; i < 5; i = i + 1) {\n print i;\n }");
    start_vm();

    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "0.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "4.000000");

    stop_vm();
}

TEST(simple_vm_test_with_nested_functions) {
    struct compilation_result compilation_result = compile_standalone(
            "fun b(){\n print 1;\n }\n fun a() {\nb();\n print 2;\n}\na();");
    start_vm();

    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");

    stop_vm();
}

TEST(simple_vm_test_with_functions) {
    struct compilation_result compilation_result = compile_standalone(
            "fun sumar(a, b) {\nvar c = a + b;\nreturn c;\n}\nprint sumar(1, 2);");
    start_vm();

    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");

    stop_vm();
}

TEST(simple_vm_test_with_scope_variables) {
    struct compilation_result compilation_result = compile_standalone(
            "var edad = 10;\n{\nvar nombre = 1;\nnombre = 1.0;\nprint nombre;}");
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == EXIT_SUCCESS);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");

    stop_vm();
}