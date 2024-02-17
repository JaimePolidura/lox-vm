#include "vm_thread_id_pool.h"

void init_thread_id_pool(struct thread_id_pool * pool) {
    pool->last = MAX_REUSABLE_THREAD_IDS_POOL;
    for(int i = 0; i < MAX_REUSABLE_THREAD_IDS_POOL; i++){
        pool->slots[i].taken = false;
    }
}

lox_thread_id acquire_thread_id_pool(struct thread_id_pool * pool) {
    uint8_t slot = ((unsigned long) pthread_self()) & (MAX_REUSABLE_THREAD_IDS_POOL - 1);

    if(!pool->slots[slot].taken) {
        bool expected = false;
        if(atomic_compare_exchange_strong(&pool->slots[slot].taken, &expected, true)) {
            return slot;
        }
    }

    //Slow path
    return atomic_fetch_add(&pool->last, 1) + 1;
}

void release_thread_id_pool(struct thread_id_pool * pool, lox_thread_id to_release) {
    if(to_release < MAX_REUSABLE_THREAD_IDS_POOL){
        pool->slots[to_release].taken = false;
    }
}