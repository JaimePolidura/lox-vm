#pragma once

#include "test.h"
#include "runtime/vm.h"
#include "compiler/bytecode/bytecode_compiler.h"

extern struct vm current_vm;
extern struct trie_list * compiled_packages;
extern const char * compiling_base_dir;

#ifdef USING_GEN_GC_ALG
TEST (simple_vm_test_major_gc) {
    start_vm();
    
    interpret_result_t result = interpret_vm(compile_bytecode(
            "struct Persona{"
            "   nombre;"
            "   edad;"
            "}"
            ""
            "var jaime = Persona{\"Jaime\", 21};"
            "forceGC();"
            "var molon = Persona{\"Molon\", 19};"
            "forceGC();"
            "var juanito = Persona{\"Juanito\", 11};"
            "forceGC();"
            ""
            "print jaime.edad;"
            "print molon.edad;"
            "print juanito.edad;",
            "main", NULL));

    ASSERT_TRUE(result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "19.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "11.000000");

    stop_vm();
    reset_vm();
}
#endif

TEST (simple_vm_test_gc_old_gen) {
    start_vm();

    interpret_result_t result = interpret_vm(compile_bytecode(
            "struct Persona{"
            "   nombre;"
            "   edad;"
            "}"
            ""
            "var jaime = Persona{\"Jaime\", 21};"
            "var walo = Persona{\"Walo\", 19};"
            "forceGC();"
            "var molon = Persona{\"Molon\", 19};"
            "forceGC();"
            "forceGC();"
            "forceGC();"
            ""
            "print jaime.edad;"
            "print jaime.nombre;"
            "print molon.edad;"
            "print molon.nombre;",
            "main", NULL));

    ASSERT_TRUE(result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "Jaime");

    ASSERT_NEXT_VM_LOG(current_vm, "19.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "Molon");

    stop_vm();
    reset_vm();
}

TEST (simple_vm_test_gc_local_objects_may_be_moved) {
    start_vm();

    interpret_result_t result = interpret_vm(compile_bytecode(
            "struct Persona{"
            "   nombre;"
            "   edad;"
            "}"
            ""
            "fun function() {"
            "   var jaime = Persona{\"Jaime\", 21};"
            "   print jaime.edad;"
            "   forceGC();"
            "   var molon = Persona{\"Molon\", 19};"
            "   print jaime.edad;"
            "   print molon.edad;"
            "   forceGC();"
            "   print jaime.edad;"
            "   print molon.edad;"
            "}"
            ""
            "function();"
            "",
            "main", NULL));

    ASSERT_TRUE(result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "19.000000");

    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "19.000000");

    stop_vm();
    reset_vm();
}

TEST (simple_vm_test_gc_global_objects_may_be_moved) {
    start_vm();

    interpret_result_t result = interpret_vm(compile_bytecode(
            "struct Persona{"
            "   nombre;"
            "   edad;"
            "}"
            ""
            "var jaime = Persona{\"Jaime\", 21};"
            "print jaime.edad;"
            "forceGC();"
            "var molon = Persona{\"Molon\", 19};"
            "print jaime.edad;"
            "print molon.edad;"
            "forceGC();"
            "print jaime.edad;"
            "print molon.edad;"
            "forceGC();",
            "main", NULL));

    ASSERT_TRUE(result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "19.000000");

    ASSERT_NEXT_VM_LOG(current_vm, "21.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "19.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_threads_gc){
    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\gc\\main.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\gc",
            "main"
    );

    start_vm();

    interpret_result_t vm_result = interpret_vm(result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_TRUE(current_vm.last_gc_result.bytes_allocated_before_gc > current_vm.last_gc_result.bytes_allocated_after_gc);

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_threads_no_race_condition) {
    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\threads_race_conditions\\sync_no_race_condition.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\threads_race_conditions",
            "main"
    );

    start_vm();

    interpret_result_t vm_result = interpret_vm(result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    //4 threads_race_conditions, 100000 million increments per each thread. Really unlikely that the final result will be 400000
    ASSERT_TRUE(strtod(current_vm.log[0], NULL) == 40000);
    ASSERT_TRUE(strtod(current_vm.log[1], NULL) == 40000);

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_threads_race_condition){
    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\threads_race_conditions\\race_condition.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\threads_race_conditions",
            "main"
    );

    start_vm();

    interpret_result_t vm_result = interpret_vm(result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    //4 threads_race_conditions, 100000 million increments per each thread. Really unlikely that the final result will be 400000
    ASSERT_TRUE(strtod(current_vm.log[0], NULL) != 4000000);

    stop_vm();
    reset_vm();
}


TEST(simple_vm_test_threads_join){
    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\threads_join\\join.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\threads_join",
            "main"
    );

    start_vm();

    interpret_result_t vm_result = interpret_vm(result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_TRUE(strtod(current_vm.log[0], NULL) >= 300);

    stop_vm();
    reset_vm();
}

TEST(vm_global_functions_test){
    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\global_functions\\main.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\global_functions",
            "main"
    );

    start_vm();

    interpret_result_t vm_result = interpret_vm(result);
    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "Opened file 1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "reading 10.000000 bytes from file 1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "closing file 1.000000");

    stop_vm();
    reset_vm();
}

TEST(vm_file_global_structs_test) {
    reset_vm();

    struct compilation_result result = compile(
            "C:\\programacion\\lox-vm\\tst\\resources\\global_structs\\main.lox",
            "C:\\programacion\\lox-vm\\tst\\resources\\global_structs",
            "main"
    );
    start_vm();

    interpret_result_t vm_result = interpret_vm(result);
    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "192.168.1.159");
    ASSERT_NEXT_VM_LOG(current_vm, "8080.000000");

    stop_vm();
    reset_vm();
}

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
    ASSERT_NEXT_VM_LOG(current_vm, "10.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "5.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");

    stop_vm();
    reset_vm();
}

TEST (simple_vm_test_inline_array_initilization) {
    struct compilation_result compilation_result = compile_standalone(
            "var array = [1, 2, 3, 4];"
            "print array[2];"
            "var array2[10]; "
            "array2[9] = 10; "
            "array[0] = array2[9];"
            "print array[0];"
    );

    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);

    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "10.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_empty_array_initilization) {
    struct compilation_result compilation_result = compile_standalone(
            "var array[10]; "
    );

    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_structs) {
    struct compilation_result compilation_result = compile_standalone(
            "struct Persona{ "
                "nombre;"
                "edad;"
            "}"
            "var jaime = Persona{\"Jaime\", 21}; "
            "print jaime.nombre; "
            "jaime.edad = 21 + 1;"
            " print jaime.edad;"
    );
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "Jaime");
    ASSERT_NEXT_VM_LOG(current_vm, "22.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_while) {
    struct compilation_result compilation_result = compile_standalone(
            "var i = 0;"
            "while(i < 2) { "
            "   print i; i = i + 1;"
            "}");
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "0.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_ifs) {
    struct compilation_result compilation_result = compile_standalone(
            "if(1 == 1) {"
               "print 1; "
            "}"
            "if(1 == 2) {"
                "print 2;"
            "} else { "
                "print 3; "
            "}"
            "if (1 == 2) {"
                "print 4; "
            "}"
            "print 5;");
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "5.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_for_loops) {
    struct compilation_result compilation_result = compile_standalone(
            "for(var i = 0; i < 5; i = i + 1) { "
                "print i; "
            "}");
    start_vm();

    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "0.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "4.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_nested_functions) {
    struct compilation_result compilation_result = compile_standalone(
            "fun b(){"
                "print 1; "
            "} "
            "fun a() {"
                "b(); "
                "print 2;"
            "}"
            "a();"
    );
    start_vm();

    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");
    ASSERT_NEXT_VM_LOG(current_vm, "2.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_functions) {
    struct compilation_result compilation_result = compile_standalone(
            "fun sumar(a, b) {"
                "var c = a + b;"
                "return c;"
             "}"
             "print sumar(1, 2);");
    start_vm();

    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == INTERPRET_OK);
    ASSERT_NEXT_VM_LOG(current_vm, "3.000000");

    stop_vm();
    reset_vm();
}

TEST(simple_vm_test_with_scope_variables) {
    struct compilation_result compilation_result = compile_standalone(
            "var edad = 10;\n"
            "{"
                "var nombre = 1;"
                "nombre = 1.0;"
                "print nombre;"
            "}");
    start_vm();
    interpret_result_t vm_result = interpret_vm(compilation_result);

    ASSERT_TRUE(vm_result == EXIT_SUCCESS);
    ASSERT_NEXT_VM_LOG(current_vm, "1.000000");

    stop_vm();
    reset_vm();
}