#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"

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

}