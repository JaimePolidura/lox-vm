#include "shared.h"
#include "compiler/bytecode_compiler.h"
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
            "   resta(a, b);"
            "}"
            ""
            "calcular(1, 2);",
    "main", NULL);

    create_call_graph(&result);
}