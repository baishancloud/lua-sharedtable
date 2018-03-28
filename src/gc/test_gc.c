#include <stdlib.h>
#include "gc.h"
#include "table/table.h"
#include "unittest/unittest.h"

#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>

#include <sys/sem.h>
#include <sys/ipc.h>
#include <time.h>
#include <stdlib.h>

#define TEST_POOL_SIZE 100 * 1024 * 4096

//copy from slab
static int _slab_size_to_index(uint64_t size) {
    if (size <= (1 << ST_SLAB_OBJ_SIZE_MIN_SHIFT)) {
        return ST_SLAB_OBJ_SIZE_MIN_SHIFT;
    }

    if (size > (1 << ST_SLAB_OBJ_SIZE_MAX_SHIFT)) {
        return ST_SLAB_GROUP_CNT - 1;
    }

    size--;

    return st_bit_msb(size) + 1;
}

static ssize_t get_alloc_cnt_in_slab(st_table_pool_t *pool, uint64_t size) {

    int idx = _slab_size_to_index(size);

    return pool->slab_pool.groups[idx].stat.current.alloc.cnt;
}

static ssize_t remain_table_cnt(st_table_pool_t *pool) {
    return get_alloc_cnt_in_slab(pool, sizeof(st_table_t));
}

static st_table_pool_t *alloc_table_pool() {
    st_table_pool_t *pool = mmap(NULL, TEST_POOL_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    st_assert(pool != MAP_FAILED);

    memset(pool, 0, sizeof(st_table_pool_t));

    void *data = (void *)(st_align((uintptr_t)pool + sizeof(st_table_pool_t), 4096));

    int region_size = 1024 * 4096;
    int ret = st_region_init(&pool->slab_pool.page_pool.region_cb, data,
                             region_size / 4096, TEST_POOL_SIZE / region_size, 0);
    st_assert(ret == ST_OK);

    ret = st_pagepool_init(&pool->slab_pool.page_pool, 4096);
    st_assert(ret == ST_OK);

    ret = st_slab_pool_init(&pool->slab_pool);
    st_assert(ret == ST_OK);

    ret = st_table_pool_init(pool, 1);
    st_assert(ret == ST_OK);

    return pool;
}

static void free_table_pool(st_table_pool_t *pool) {

    int ret = st_table_pool_destroy(pool);
    st_assert(ret == ST_OK);

    ret = st_slab_pool_destroy(&pool->slab_pool);
    st_assert(ret == ST_OK);

    ret = st_pagepool_destroy(&pool->slab_pool.page_pool);
    st_assert(ret == ST_OK);

    ret = st_region_destroy(&pool->slab_pool.page_pool.region_cb);
    st_assert(ret == ST_OK);

    munmap(pool, TEST_POOL_SIZE);
}

static void add_sub_table(st_table_t *table, char *name, st_table_t *sub) {

    char key_buf[11] = {0};
    int value_buf[40] = {0};

    memcpy(key_buf, name, strlen(name));
    st_str_t key = st_str_wrap(key_buf, sizeof(key_buf));

    memcpy(value_buf, &sub, (size_t)sizeof(sub));
    st_str_t value = st_str_wrap_common(value_buf, ST_TYPES_TABLE, sizeof(value_buf));

    st_assert(st_table_add_key_value(table, key, value) == ST_OK);
}

static void gc_clean_all(st_table_pool_t *table_pool) {

    while (1) {
        int ret = st_gc_run(&table_pool->gc);
        if (ret == ST_NO_GC_DATA) {
            break;
        }

        st_assert(ret == ST_OK);
    }
}

static void clean_root_table(st_table_t *root) {

    st_table_pool_t *table_pool = root->pool;
    char key_buf[11] = {0};
    int value_buf[40] = {0};
    int element_size = sizeof(st_table_element_t) + sizeof(key_buf) + sizeof(value_buf);

    st_assert(st_table_remove_all(root) == ST_OK);

    gc_clean_all(table_pool);

    st_assert(st_gc_remove_root(&table_pool->gc, &root->gc_head) == ST_OK);
    st_assert(st_table_free(root) == ST_OK);

    st_assert(remain_table_cnt(table_pool) == 0);
    st_assert(get_alloc_cnt_in_slab(table_pool, element_size) == 0);
}

static void add_tables_into_root(st_table_t *root, int cnt, int sub_cnt) {

    st_table_t *t1;
    st_table_t *t2;
    char key_buf[11] = {0};

    for (int i = 0; i < cnt; i++) {
        sprintf(key_buf, "%010d", i);
        st_assert(st_table_new(root->pool, &t1) == ST_OK);
        add_sub_table(root, key_buf, t1);

        for (int j = 0; j < sub_cnt; j++) {
            sprintf(key_buf, "%010d", j);
            st_assert(st_table_new(root->pool, &t2) == ST_OK);
            add_sub_table(t1, key_buf, t2);
        }
    }
}

static void run_gc_round(st_gc_t *gc) {

    int usec;
    int ret;
    int64_t start = 0;
    int64_t end = 0;

    do {
        st_time_in_usec(&start);
        ret = st_gc_run(gc);
        st_time_in_usec(&end);

        st_assert(ret == ST_OK || ret == ST_NO_GC_DATA);

        usec = end - start;

        st_assert(0 < usec && usec < 5000);

        st_assert(gc->max_visit_cnt > 10);
        st_assert(gc->max_free_cnt > 10);

    } while (gc->begin);
}

st_test(table, push_to_mark_and_sweep_queue) {

    st_table_t *t;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    for (int i = 0; i < 100; i++) {
        st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");
        st_ut_eq(ST_OK, st_gc_push_to_mark(gc, &t->gc_head), "");

        //test if table already marked, return state.
        t->gc_head.mark = st_gc_status_reachable(gc);
        st_ut_eq(ST_OK, st_gc_push_to_mark(gc, &t->gc_head), "");

        //test if table already in mark_queu, return state.
        t->gc_head.mark = st_gc_status_unknown(gc);
        st_ut_eq(ST_OK, st_gc_push_to_mark(gc, &t->gc_head), "");

        st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
    }

    int i = 0;
    st_list_t *node;
    st_list_for_each(node, &gc->mark_queue) {
        i++;
    }

    st_ut_eq(100, i, "");

    i = 0;
    st_list_for_each(node, &gc->sweep_queue) {
        i++;
    }

    st_ut_eq(100, i, "");

    st_ut_eq(0, st_list_empty(&gc->mark_queue), "");
    st_ut_eq(1, st_list_empty(&gc->prev_sweep_queue), "");
    st_ut_eq(0, st_list_empty(&gc->sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->garbage_queue), "");

    st_ut_eq(100, remain_table_cnt(table_pool), "");

    free_table_pool(table_pool);
}

st_test(table, run_gc_from_begin) {

    st_table_t *t;
    st_table_t *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    // if no table to sweep, no need to run gc.
    st_ut_eq(ST_NO_GC_DATA, st_gc_run(gc), "");

    st_table_new(table_pool, &t);
    st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");

    st_ut_eq(0, gc->begin, "");
    st_ut_eq(1, st_list_empty(&gc->mark_queue), "");
    st_ut_eq(0, st_list_empty(&gc->sweep_queue), "");

    add_tables_into_root(root, 100, 10);

    // let gc stop before free table in garbage_queue phase.
    gc->max_free_cnt = 0;

    st_ut_eq(ST_OK, st_gc_run(gc), "");

    st_ut_eq(1, gc->begin, "");
    st_ut_eq(0, st_list_empty(&gc->mark_queue), "");
    st_ut_eq(0, st_list_empty(&gc->sweep_queue), "");

    gc->max_free_cnt = 50;
    clean_root_table(root);

    free_table_pool(table_pool);
}

static int check_table_children_reachable(st_table_t *table, st_gc_t *gc) {

    st_str_t key;
    st_str_t value;
    st_table_iter_t iter;

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_iter_init(table, &iter, NULL, 0);
    if (ret != ST_OK) {
        goto quit;
    }

    while (1) {
        ret = st_table_iter_next(table, &iter, &key, &value);
        if (ret != ST_OK) {
            if (ret == ST_NOT_FOUND) {
                ret = ST_OK;
            }
            goto quit;
        }

        st_table_t *t = st_table_get_table_addr_from_value(value);

        if (t->gc_head.mark != st_gc_status_reachable(gc)) {
            ret = ST_STATE_INVALID;
            goto quit;
        }

        ret = check_table_children_reachable(t, gc);
        if (ret != ST_OK) {
            goto quit;
        }
    }

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}

st_test(table, mark_reachable_table) {

    st_table_t *t, *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    add_tables_into_root(root, 100, 100);

    st_table_new(table_pool, &t);
    st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");

    int i = 0;
    int64_t start = 0;
    int64_t end = 0;

    // let gc stop before free table in garbage_queue phase.
    gc->max_free_cnt = 0;

    do {
        st_time_in_usec(&start);
        st_ut_eq(ST_OK, st_gc_run(gc), "");
        st_time_in_usec(&end);

        st_ut_lt(end - start, 5000, "");
        st_ut_gt(end - start, 1, "");
        i++;
    } while (!st_list_empty(&gc->mark_queue));

    st_ut_gt(i, 1, "");

    st_ut_eq(ST_OK, check_table_children_reachable(root, gc), "");

    gc->max_free_cnt = 50;
    clean_root_table(root);

    free_table_pool(table_pool);
}

st_test(table, mark_in_sweep_queue) {

    st_table_t *t, *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    add_tables_into_root(root, 100, 100);

    // push tables to sweep queue
    for (int i = 0; i < 50; i++) {
        st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");
        st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
    }

    // let gc stop before free table in garbage_queue phase.
    gc->max_free_cnt = 0;

    // after gc run, tables move from sweep_queue into garbage_queue.
    do {
        st_ut_eq(ST_OK, st_gc_run(gc), "");
    } while (!st_list_empty(&gc->mark_queue) || !st_list_empty(&gc->sweep_queue));

    st_ut_eq(1, st_list_empty(&gc->prev_sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->remained_queue), "");

    st_ut_eq(0, st_list_empty(&gc->garbage_queue), "");

    st_gc_head_t *gc_head;

    int i = 0;
    st_list_for_each_entry(gc_head, &gc->garbage_queue, sweep_lnode) {
        st_ut_eq(st_gc_status_garbage(gc), gc_head->mark, "");
        i++;
    }

    st_ut_eq(50, i, "");

    gc->max_free_cnt = 50;
    clean_root_table(root);

    free_table_pool(table_pool);
}

st_test(table, mark_in_prev_sweep_queue) {

    st_table_t *t;
    st_table_t *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    add_tables_into_root(root, 100, 100);

    // push 1 table to sweep queue.
    st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");
    st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");

    // push 50 tables to mark_queue and sweep_queue
    // the 50 tables will be freed in the second gc round.
    for (int i = 0; i < 50; i++) {
        st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");
        st_ut_eq(ST_OK, st_gc_push_to_mark(gc, &t->gc_head), "");
        st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
    }

    // let gc stop before free table in garbage_queue phase.
    gc->max_free_cnt = 0;

    //after gc run, 50 tables moved from sweep_queue to remained_queue.
    do {
        st_ut_eq(ST_OK, st_gc_run(gc), "");
    } while (!st_list_empty(&gc->mark_queue) || !st_list_empty(&gc->sweep_queue));

    int i = 0;
    st_gc_head_t *gc_head;
    st_list_for_each_entry(gc_head, &gc->remained_queue, sweep_lnode) {
        st_ut_eq(st_gc_status_reachable(gc), gc_head->mark, "");
        i++;
    }

    st_ut_eq(50, i, "");

    // 1 table only pushed to sweep queue, will moved into garbage_queue.
    st_ut_eq(0, st_list_empty(&gc->garbage_queue), "");

    gc->max_free_cnt = 50;
    int prev_reachable_mark = st_gc_status_reachable(gc);

    // run gc last step, 50 tables will move into prev_sweep_queue
    // 1 table will be freed.
    st_ut_eq(ST_OK, st_gc_run(gc), "");

    st_ut_eq(1, st_list_empty(&gc->mark_queue), "");
    st_ut_eq(1, st_list_empty(&gc->sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->remained_queue), "");
    st_ut_eq(1, st_list_empty(&gc->garbage_queue), "");

    i = 0;
    st_list_for_each_entry(gc_head, &gc->prev_sweep_queue, sweep_lnode) {
        st_ut_eq(prev_reachable_mark, gc_head->mark, "");
        i++;
    }
    st_ut_eq(50, i, "");

    // first gc round has been finished.
    st_ut_eq(0, gc->begin, "");

    // let gc stop before free table in garbage_queue phase.
    gc->max_free_cnt = 0;

    // run next gc round.
    // after run gc, 50 tables will move from prev_sweep_queue into garbage_queue.
    do {
        st_ut_eq(ST_OK, st_gc_run(gc), "");
    } while (!st_list_empty(&gc->mark_queue) || !st_list_empty(&gc->prev_sweep_queue));

    st_ut_eq(1, st_list_empty(&gc->mark_queue), "");
    st_ut_eq(1, st_list_empty(&gc->sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->prev_sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->remained_queue), "");

    i = 0;
    st_list_for_each_entry(gc_head, &gc->garbage_queue, sweep_lnode) {
        st_ut_eq(st_gc_status_garbage(gc), gc_head->mark, "");
        i++;
    }

    st_ut_eq(50, i, "");

    gc->max_free_cnt = 50;

    clean_root_table(root);

    free_table_pool(table_pool);
}

st_test(table, free_garbage_table) {

    st_table_t *t;
    st_table_t *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    // current has created 1101 tables.
    add_tables_into_root(root, 100, 10);

    char key_buf[11] = {0};

    for (int i = 0; i < 50; i++) {
        sprintf(key_buf, "test_%05d", i);
        st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");
        add_sub_table(root, key_buf, t);

        //push 50 tables to sweep queue, but the tables has added into root.
        st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
    }

    for (int i = 0; i < 50; i++) {
        st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");

        //push 50 tables to sweep queue, the tables will be free.
        st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
    }

    // current has created 1201 tables.
    st_ut_eq(1201, remain_table_cnt(table_pool), "");

    // after gc run, the 50 tables have not added into root, will be freed.
    run_gc_round(gc);

    st_ut_eq(1, st_list_empty(&gc->mark_queue), "");
    st_ut_eq(0, st_list_empty(&gc->prev_sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->sweep_queue), "");
    st_ut_eq(1, st_list_empty(&gc->garbage_queue), "");
    st_ut_eq(1, st_list_empty(&gc->remained_queue), "");

    st_ut_eq(1151, remain_table_cnt(table_pool), "");

    clean_root_table(root);
    free_table_pool(table_pool);
}


st_test(table, run_gc_many_rounds) {

    st_table_t *t, *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    add_tables_into_root(root, 100, 10);
    // current has created 1101 tables.

    for (int i = 0; i < 10; i++) {

        for (int j = 0; j < 10; j++) {
            st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");

            // push 10 tables to mark_queue and sweep_queue
            // the 10 tables will be freed in the second gc round.
            st_ut_eq(ST_OK, st_gc_push_to_mark(gc, &t->gc_head), "");
            st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
        }

        for (int j = 0; j < 10; j++) {
            // push 10 tables to sweep_queue
            // the 10 tables will be freed in the first gc round.
            st_ut_eq(ST_OK, st_table_new(table_pool, &t), "");
            st_ut_eq(ST_OK, st_gc_push_to_sweep(gc, &t->gc_head), "");
        }

        st_ut_eq(1121, remain_table_cnt(table_pool), "");

        int old_round = gc->round;

        for (int j = 0; j < 2; j++) {
            run_gc_round(gc);

            if (j == 0) {
                // after first gc round, 10 tables has been freed.
                st_ut_eq(1111, remain_table_cnt(table_pool), "");
                st_ut_eq(old_round + 4, gc->round, "");
            } else {
                // after second gc round, the other 10 tables has been freed.
                st_ut_eq(1101, remain_table_cnt(table_pool), "");
                st_ut_eq(old_round + 8, gc->round, "");
            }
        }

        st_ut_eq(1, st_list_empty(&gc->mark_queue), "");
        st_ut_eq(1, st_list_empty(&gc->prev_sweep_queue), "");
        st_ut_eq(1, st_list_empty(&gc->sweep_queue), "");
        st_ut_eq(1, st_list_empty(&gc->garbage_queue), "");
    }

    clean_root_table(root);
    free_table_pool(table_pool);
}

st_test(table, destroy_gc) {

    st_table_t *root;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_gc_t *gc = &table_pool->gc;

    st_table_new(table_pool, &root);
    st_ut_eq(ST_OK, st_gc_add_root(gc, &root->gc_head), "");

    add_tables_into_root(root, 100, 10);

    st_ut_eq(ST_OK, st_table_remove_all(root), "");

    st_ut_eq(ST_OK, st_gc_remove_root(&table_pool->gc, &root->gc_head), "");
    st_ut_eq(ST_OK, st_table_free(root), "");

    st_ut_eq(ST_OK, st_gc_destroy(&table_pool->gc), "");

    st_ut_eq(0, remain_table_cnt(table_pool), "");
}

st_ut_main;
