#pragma once

#include "test.h"
#include "runtime/vm.h"
#include "compiler/compiler.h"
#include "compiler/chunk/chunk_disassembler.h"

extern struct vm current_vm;

TEST(vm_jit_struct){
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "struct Persona {"
            "   nombre;"
            "   edad;"
            "}"
            ""
            "fun function() {"
            "   var jaime = Persona{\"Jaime\", 21};"
            "   print jaime.nombre;"
            "   jaime.edad = 2;"
            ""
            "   return jaime;"
            "}"
            ""
            "forceJIT(function);"
            ""
            "print function().edad;"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "Jaime");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
}

TEST(vm_jit_arrays) {
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun function() {"
            "   var array = [1, 2, 3, 4];"
            "   print array[1];"
            "   array[0] = array[3];"
            "   print array[0];"
            "}"
            ""
            "forceJIT(function);"
            ""
            "function();"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "4.000000");
}

TEST(vm_jit_globals){
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "var numero = 1;"
            "fun function() {"
            "   print numero;"
            "   numero = 2;"
            "}"
            ""
            "forceJIT(function);"
            ""
            "function();"
            "print numero;"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
}

TEST(vm_jit_monitors) {
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun sync function() {"
            "   sync {"
            "       print 1;"
            "   }"
            "   sync {"
            "       print 2;"
            "   }"
            "}"
            ""
            "forceJIT(function);"
            ""
            "function();"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
}

TEST(vm_jit_for_loop){
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun sum(n) {"
            "   var result = 0;"
            "   for(var i = 0; i < n; i = i + 1) {"
            "       result = result + n;"
            "   }"
            "   return result;"
            "}"
            ""
            "forceJIT(sum);"
            ""
            "print sum(84);"
    );

    interpret_vm(compilation);
    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "7056.000000");
}

TEST(vm_jit_if_test) {
    start_vm();

    struct compilation_result compilation = compile_standalone(
            "fun hacerPrint(a) {"
            "   if(a == 1) {"
            "       print a;"
            "   }"
            "   if(a == 2) {"
            "       print a;"
            "   }"
            "}"
            "forceJIT(hacerPrint);"
            ""
            "hacerPrint(2);"
            "hacerPrint(1);"
    );

    interpret_vm(compilation);

    stop_vm();
    reset_vm();

    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
}

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