#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"
#include "compiler/compiler.h"

int main() {
    puts("Sin inline:");
    auto result = compile_bytecode(
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
            "print 1;",
    "main", NULL);

    disassemble_package(result.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);

//    printf("\n\n\n");
//    puts("Con inline:");
//
//    auto result2 = compile_standalone(
//            "fun suma(a, b) {"
//            "   return a + b;"
//            "}"
//            ""
//            "for(var i = 0; i < 5; i = i + 1) {"
//            "   print inline suma(i, i);"
//            "}"
//            "print 1;"
//    );
//
//    disassemble_package(result2.compiled_package, DISASSEMBLE_ONLY_MAIN);
}