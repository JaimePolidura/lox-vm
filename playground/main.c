#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"
#include "compiler/compiler.h"

int main() {
    puts("Con inline:");
    auto result1 = compile_bytecode(
            "fun numero(a) {"
            "   if (a > 100) {"
            "       print 1;"
            "   } else {"
            "       print 2;"
            "   }"
            "}"
            ""
            "print inline numero(101);",
            "main", NULL);

    disassemble_package(result1.compiled_package, DISASSEMBLE_PACKAGE_FUNCTIONS);
    puts("\n\n");

    puts("Sin inline:");
    auto result2 = compile_standalone(
            "fun numero(a) {"
            "   if (a > 100) {"
            "       return 1;"
            "   } else {"
            "       return 2;"
            "   }"
            "}"
            ""
            "print inline numero(101);");

    disassemble_package(result2.compiled_package, DISASSEMBLE_ONLY_MAIN);
}