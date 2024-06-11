#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"
#include "compiler/compiler.h"

int main() {
    auto result = compile_bytecode(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            ""
            "print inline suma(1, 2);",
    "main", NULL);

    disassemble_package(result.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);

    printf("\n\n\n");

    auto result2 = compile_standalone(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            ""
            "print inline suma(1, 2);");

    disassemble_package(result2.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);
}