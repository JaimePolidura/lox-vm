#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"

int main() {
    auto result = compile_bytecode(
            "fun suma(a, b) {"
            "}"
            ""
            "fun resta(a, b) {"
            "   suma(a, b);"
            "}"
            ""
            "fun calcular(a, b) {"
            "   suma(a, b);"
            "   inline resta(a, b);"
            "}"
            ""
            "inline calcular(1, 2);",
    "main", NULL);

    disassemble_package(result.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);

    struct call_graph * call_graph = create_call_graph(&result);

    puts("hola");
}