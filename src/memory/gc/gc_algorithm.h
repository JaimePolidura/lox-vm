#pragma once

//Every gc algorithm will have to implement this methods

//Called by vm when it reaches a "safe point". Used for stop-the-world pauses
void check_gc_on_safe_point_alg();

//Signal to threads_race_conditions that a gc have started. This won't block threads_race_conditions.
//If a thread wants to check if there is pending gc, it will check via calling check_gc_on_safe_point_alg()
void signal_threads_start_gc_alg();

//Init all struct fields for the gc algorithm. Only called once at boot time
void init_gc_alg();

//Starting a gc
void start_gc_alg();

//Signal threads_race_conditions that the gc have finished
void signal_threads_gc_finished_alg();