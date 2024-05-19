#include "shared.h"
#include "compiler/bytecode_compiler.h"
#include "compiler/chunk/chunk_disassembler.h"

int main() {
    auto result = compile_standalone(
            "fun suma(a, b) {"
            "   return a + b;"
            "}"
            ""
            "fun resta(a, b) {"
            "   return a - b;"
            "}"
            ""
            "fun calcular(a, b) {"
            "   var c = suma(a, b);"
            "   var d = resta(a, b);"
            "   return c * d;"
            "}"
    );

    disassemble_package(result.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);
}