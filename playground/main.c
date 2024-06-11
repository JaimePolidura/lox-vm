#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"
#include "compiler/compiler.h"

int main() {
    puts("Sin inline:");
    auto result = compile_bytecode(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            "var a = inline suma(1, 2);"
            "var b = inline suma(3, 3);"
            "print a + b;",
    "main", NULL);

    disassemble_package(result.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);

    printf("\n\n\n");
    puts("Con inline:");

    auto result2 = compile_standalone(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            "var a = inline suma(1, 2);"
            "var b = inline suma(3, 3);"
            "print a + b;"
    );

    disassemble_package(result2.compiled_package, DISASSEMBLE_ONLY_MAIN);
}