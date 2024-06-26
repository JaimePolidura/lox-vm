#include "shared.h"
#include "compiler/bytecode/bytecode_compiler.h"
#include "compiler/inline/call_graph.h"
#include "compiler/compiler.h"
#include "runtime/vm.h"

int main() {
    start_vm();

    interpret_vm(compile_bytecode(
            "struct Persona{"
            "   nombre;"
            "   edad;"
            "}"
            ""
            "var jaime = Persona{\"Jaime\", 21};"
            "var jaime2 = Persona{\"Jaime\", 22};"
            "var jaime3 = Persona{\"Jaime\", 23};"
            "var jaime4 = Persona{\"Jaime\", 24};"
            "print jaime.edad;"
            "print jaime2.edad;"
            "print jaime3.edad;"
            "print jaime4.edad;",
            "main", NULL));

    stop_vm();
}