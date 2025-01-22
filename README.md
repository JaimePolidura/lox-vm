# lox-vm

An implementation of lox pending_bytecode interpreter described in "Crafting Interpreters" with a jit compiler, packages, threads etc.

## New features
- <b>JIT Compiler</b> When a function is called muliple times, it will start recording runtime information like types, number of times that a branch is taken etc. When it has collected enough runtime information, it will compile the function bytecode to assembly code, More info [JIT Compiler](#JIT Compiler) section.
- <b>Threads (vm_thread.h)</b> When a function is called with "parallel" prefix, it will be run in other thread. Modified the garbage collector to run safely with multiple threads. Also implemented java l monitors value_as a synchronization tool. 
- <b>Packages (package.h)</b> Lox programs can consist of multiple files each of which can have public and private members (structs, methods and global variables)
- <b>Generational gc (generational_gc.h minor_gc.c major_gc.c)</b> Implemented eden, survivor and memory regions with write barriers, card tables, mark compact, mark copy algorithms, mark bitmaps etc.
- <b>Compile time inlining (inliner.h)</b> When a function is called with "inline" prefix, the function's pending_bytecode will get merged with the function's caller pending_bytecode.
- <b>Arrays</b>

## Features from "Crafting Interpreters" not implemented
- <b>OOP</b> Inheritnace, classess with properties and methods are not implemented. Instead, they are replaced with C like structs, which only contains plain profile_data.
- <b>Closures</b>

## JIT Compiler
- When a function is called multiple times, the runtime will start recording runtime information like types, number of times that a branch is taken etc.
- When enough runtime information is collected, the function will get compiled to assembly code.
- The compiled can be divided into a series of phases:
  - 1º <b>IR Creation</b> Translation of the function bytecode to an internal IR (<b>ssa_creator.h</b>). The IR will have the SSA form. This IR will be composed of:
    - Blocks (<b>lox_ir_block.h</b>) Series of control node that will run without branches (sequentally).
    - Control (<b>ssa_control_node.h</b>) Instructions that represents control flow (statements). This nodes might contains data flow nodes.
    - Data (<b>lox_ir_data_node.h</b>) Instructions that represents data flow (expressions)
  - 2º <b>Optimizations</b> Once it is translated to SSA IR a series of optimizations will be done:
    - Copy propagation
    - Common subexpression elimination
    - Constant propagation
    - Loop invariant code motion
    - Escape analysis
    - Type propagation/analysis
    - Box/Unbox insertion
    - Range check elimination
  - 3º <b> Virtual register allocation </b>
  - 4º <b> IR Lowering </b>
- The machine code generated will take advantaje of the runtime information collected. The IR will contain guard nodes that will check if the conditions are hold, if they are not, the bytecode interpreter will run the function. For example the first node in a function can be used for checking if an argument has type number. 

## Examples
connection.lox
```lox
pub struct Connection {
    ip;
    port;
    writeBuffer;
    readBuffer;
}

pub fun acceptConnections() {
    sleep(500);

    var readBuffer[1024];
    var writeBuffer[1024];

    return Connection{
        "192.168.1.159",
        8888,
        readBuffer,
        writeBuffer
    };
}

pub fun readRequest(connection) {
    return nil;
}
```
main.lox
```lox
use "Connection.lox";

fun handleRequest(request) {
    ...
}

fun handleConnection(connection) {
    var request = inline Connection::readRequest(connection);
    inline handleRequest(request);
}

fun startServer() {
    while(true){
        var connection = Connection::acceptConnections();
        parallel handleConnection(connection);
    }
}

startServer();
```
