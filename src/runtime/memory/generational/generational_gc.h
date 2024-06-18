#pragma once

#include "runtime/memory/generational/survivor.h"
#include "runtime/memory/generational/eden.h"
#include "runtime/memory/generational/old.h"
#include "runtime/memory/gc_algorithm.h"
#include "runtime/memory/gc_result.h"


#include "runtime/threads/vm_thread.h"
#include "runtime/vm.h"

//Global struct. Maintained in vm.h
struct generational_gc {
    struct eden * eden;
    struct survivor * survivor;
    struct old * old;
};

//Pert thread. Maintained in vm_thread.h
struct generational_thread_gc {
    struct eden_thread * eden;
};