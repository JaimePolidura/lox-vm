# lox-vm

An implementation of lox bytecode interpreter described in "Crafting Interpreters" with a jit compiler, packages, threads etc.

## New features
- <b>Basic x64 JIT Compiler</b> When a function is called muliple times it is compiled into x64 native code. This is implemented in x64_jit_compiler.c and jit_compiler.c
- <b>Threads (vm_thread.h)</b> When a function is called with "parallel" prefix, it will be run in other thread. Modified the garbage collector to run safely with multiple threads. Also implemented java l monitors as a synchronization tool. 
- <b>Packages (package.h)</b> Lox programs can consist of multiple files each of which can have public and private members (structs, methods and global variables)
- <b>Generational gc (generational_gc.h minor_gc.c major_gc.c)</b> Implemented eden, survivor and memory regions with write barriers, card tables, mark compact, mark copy algorithms, mark bitmaps etc.
- <b>Compile time inlining (inliner.h)</b> When a function is called with "inline" prefix, the function's bytecode will get merged with the function's caller bytecode.
- <b>Arrays</b>

## Features from "Crafting Interpreters" not implemented
- <b>OOP</b> Inheritnace, classess with properties and methods are not implemented. Instead, they are replaced with C like structs, which only contains plain data.
- <b>Closures</b>

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
