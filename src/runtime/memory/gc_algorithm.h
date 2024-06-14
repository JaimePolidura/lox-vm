#pragma once

#include "shared.h"

//Every gc algorithm will have to implement these methods

//When a thread heap allocates an object, this function will get called.
//This function may trigger a garbage collection.
void add_object_to_heap_gc_alg(struct object * object);

//Called by runtime when it reaches a "safe point".
//Checkpoints are called periodicaly in static places by threads (at loop, function calls...)
//If there is a gc on going, the thread will get blocked.
void check_gc_on_safe_point_alg();

//Init all struct fields for the gc algorithm. Only called once at boot time
void * alloc_gc_thread_info_alg(); //The returned value is stored in vm_thread as a thread local value
void * alloc_gc_alg(); //The returned value is stored in vm.h as a global value

//Starts a gc
//May fail if there is a garbage colection going on or other thread concurretly tries to start a gc
struct gc_result try_start_gc_alg();
