#pragma once

#include "test.h"
#include "vm/threads/vm_thread_id_pool.h"

TEST(vm_thread_id_pool_same_thread_test) {
    struct thread_id_pool pool;
    init_thread_id_pool(&pool);

    lox_thread_id acquired1 = acquire_thread_id_pool(&pool);
    lox_thread_id acquired2 = acquire_thread_id_pool(&pool);

    ASSERT_TRUE(acquired1 < acquired2);

    release_thread_id_pool(&pool, acquired1);

    lox_thread_id acquired3 = acquire_thread_id_pool(&pool);

    ASSERT_TRUE(acquired3 == acquired1);
}
