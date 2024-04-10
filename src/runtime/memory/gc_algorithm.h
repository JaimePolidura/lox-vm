#pragma once

#include "shared.h"

//Every gc algorithm will have to implement these methods

//When a thread heap allocates an object, this function will get called.
//This function may trigger a garbage collection.
void add_object_to_heap_gc_alg(struct gc_thread_info * gc_thread_info, struct object * object, size_t size);

//Called by runtime when it reaches a "safe point".
//Checkpoints are called when the vm_threads's stacks are safe to manipulate (they are empty, at least they contain no heap-allocated objects)
//If there is a gc on going, the thread will get blocked.
void check_gc_on_safe_point_alg();

//Init all struct fields for the gc algorithm. Only called once at boot time
struct gc_thread_info * alloc_gc_thread_info_alg();
struct gc * alloc_gc_alg();

//Starting a gc
struct gc_result start_gc_alg();
