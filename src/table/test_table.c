#include <stdlib.h>
#include "table.h"
#include "unittest/unittest.h"

#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>

#include <sys/sem.h>
#include <sys/ipc.h>
#include <time.h>
#include <stdlib.h>

#define TEST_POOL_SIZE 100 * 1024 * 4096

#ifdef _SEM_SEMUN_UNDEFINED
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};
#endif

typedef int (*process_f)(int process_id, st_table_pool_t *pool);

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

static ssize_t get_alloc_num_in_slab(st_table_pool_t *table_pool, uint64_t size) {

    st_slab_pool_t *pool = &table_pool->slab_pool;

    int idx = _slab_size_to_index(size);

    return pool->groups[idx].stat.current.alloc.cnt;
}

static st_table_pool_t *alloc_table_pool() {
    st_table_pool_t *table_pool = mmap(NULL, TEST_POOL_SIZE, PROT_READ | PROT_WRITE,
                                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    st_assert(table_pool != MAP_FAILED);

    memset(table_pool, 0, sizeof(st_table_pool_t));

    void *data = (void *)(st_align((uintptr_t)table_pool + sizeof(st_table_pool_t), 4096));

    int region_size = 1024 * 4096;
    int ret = st_region_init(&table_pool->slab_pool.page_pool.region_cb, data,
                             region_size / 4096, TEST_POOL_SIZE / region_size, 0);
    st_assert(ret == ST_OK);

    ret = st_pagepool_init(&table_pool->slab_pool.page_pool, 4096);
    st_assert(ret == ST_OK);

    ret = st_slab_pool_init(&table_pool->slab_pool);
    st_assert(ret == ST_OK);

    ret = st_table_pool_init(table_pool);
    st_assert(ret == ST_OK);

    return table_pool;
}

static void free_table_pool(st_table_pool_t *table_pool, int with_table_pool) {

    int ret;

    if (with_table_pool) {
        ret = st_table_pool_destroy(table_pool);
        st_assert(ret == ST_OK);
    }

    ret = st_slab_pool_destroy(&table_pool->slab_pool);
    st_assert(ret == ST_OK);

    ret = st_pagepool_destroy(&table_pool->slab_pool.page_pool);
    st_assert(ret == ST_OK);

    ret = st_region_destroy(&table_pool->slab_pool.page_pool.region_cb);
    st_assert(ret == ST_OK);

    munmap(table_pool, TEST_POOL_SIZE);
}

void set_process_to_cpu(int cpu_id) {
    int cpu_cnt = sysconf(_SC_NPROCESSORS_CONF);

    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(cpu_id % cpu_cnt, &set);

    sched_setaffinity(0, sizeof(set), &set);
}

int wait_children(int *pids, int pid_cnt) {
    int ret, err = ST_OK;

    for (int i = 0; i < pid_cnt; i++) {
        waitpid(pids[i], &ret, 0);
        if (ret != ST_OK) {
            err = ret;
        }
    }

    return err;
}

void assure_alloc_struct_num_from_slab(st_table_pool_t *table_pool, int element_size, int table_num,
                                       int element_num, int process_ref_num) {

    st_assert(get_alloc_num_in_slab(table_pool, sizeof(st_table_t)) == table_num);

    st_assert(get_alloc_num_in_slab(table_pool, element_size) == element_num);

    st_assert(get_alloc_num_in_slab(table_pool, sizeof(st_refcnt_process_t)) == process_ref_num);
}

st_test(table, pool_init) {

    st_table_pool_t *table_pool = alloc_table_pool();

    st_ut_eq(table_pool, table_pool->root_table.pool, "");
    st_ut_eq(1, table_pool->root_table.inited, "");

    int64_t cnt;
    st_ut_eq(ST_OK, st_refcnt_get_total_refcnt(&table_pool->root_table.refcnt, &cnt), "");
    st_ut_eq(0, cnt, "");

    st_str_t name = st_str_const("_st_root_table");
    st_ut_eq(0, st_str_cmp(&name, &table_pool->root_table.name), "");

    st_ut_eq(1, st_rbtree_is_empty(&table_pool->root_table.elements), "");
    st_ut_eq(1, table_pool->root_table.inited, "");

    st_ut_eq(ST_ARG_INVALID, st_table_pool_init(NULL), "");

    free_table_pool(table_pool, 1);
}

st_test(table, pool_destroy) {

    st_table_t *t;
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + strlen("test") + sizeof(t);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t);
    st_ut_eq(0, st_rbtree_is_empty(&table_pool->root_table.elements), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_ut_eq(ST_OK, st_table_pool_destroy(table_pool), "");

    st_ut_eq(1, st_rbtree_is_empty(&table_pool->root_table.elements), "");
    st_ut_eq(0, table_pool->root_table.inited, "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    st_ut_eq(ST_ARG_INVALID, st_table_pool_destroy(NULL), "");

    free_table_pool(table_pool, 0);
}

st_test(table, new) {

    st_table_t *table;
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + strlen("test") + sizeof(table);

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    st_ut_eq(ST_OK, st_table_new(table_pool, (st_str_t)st_str_const("test"), &table), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_rbtree_node_t *n = st_rbtree_left_most(&table_pool->root_table.elements);
    st_table_element_t *e = st_owner(n, st_table_element_t, rbnode);
    st_table_t *t = *(st_table_t **)(e->val.bytes);
    st_ut_eq(t, table, "");

    int64_t cnt;
    st_refcnt_get_total_refcnt(&table_pool->root_table.refcnt, &cnt);
    st_ut_eq(0, cnt, "");

    st_refcnt_get_total_refcnt(&table->refcnt, &cnt);
    st_ut_eq(1, cnt, "");

    st_str_t name = st_str_const("test");
    st_ut_eq(0, st_str_cmp(&name, &table->name), "");

    st_ut_eq(1, st_rbtree_is_empty(&table->elements), "");
    st_ut_eq(1, table->inited, "");

    st_ut_eq(ST_EXISTED, st_table_new(table_pool, (st_str_t)st_str_const("test"), &t), "");
    st_refcnt_get_total_refcnt(&table->refcnt, &cnt);
    st_ut_eq(1, cnt, "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_ut_eq(ST_ARG_INVALID, st_table_new(NULL, (st_str_t)st_str_const("test"), &t), "");
    st_ut_eq(ST_ARG_INVALID, st_table_new(table_pool, (st_str_t)st_str_const("test"), NULL), "");
    st_ut_eq(ST_ARG_INVALID, st_table_new(table_pool, (st_str_t)st_str_null, &t), "");

    free_table_pool(table_pool, 1);
}

st_test(table, get_ref) {

    st_table_pool_t *table_pool = alloc_table_pool();

    int64_t cnt;
    st_table_t *table, *t;
    int element_size = sizeof(st_table_element_t) + strlen("test") + sizeof(t);

    st_ut_eq(ST_OK, st_table_new(table_pool, (st_str_t)st_str_const("test"), &table), "");

    for (int i = 1; i < 10; i++) {
        st_ut_eq(ST_OK, st_table_get_ref(table_pool, (st_str_t)st_str_const("test"), &t), "");
        st_ut_eq(t, table, "");

        st_refcnt_get_total_refcnt(&t->refcnt, &cnt);
        st_ut_eq(i + 1, cnt, "");
    }

    for (int i = 1; i < 10; i++) {
        st_table_return_ref(t);
    }

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_refcnt_get_total_refcnt(&table->refcnt, &cnt);
    st_ut_eq(1, cnt, "");

    st_ut_eq(ST_NOT_FOUND, st_table_get_ref(table_pool, (st_str_t)st_str_const("test1"), &t), "");

    st_ut_eq(ST_ARG_INVALID, st_table_get_ref(NULL, (st_str_t)st_str_const("test"), &t), "");
    st_ut_eq(ST_ARG_INVALID, st_table_get_ref(table_pool, (st_str_t)st_str_null, &t), "");
    st_ut_eq(ST_ARG_INVALID, st_table_get_ref(table_pool, (st_str_t)st_str_null, NULL), "");

    free_table_pool(table_pool, 1);
}

st_test(table, return_ref) {

    st_table_pool_t *table_pool = alloc_table_pool();

    int64_t cnt;
    st_table_t *table, *t;
    int element_size = sizeof(st_table_element_t) + strlen("test") + sizeof(table);

    st_ut_eq(ST_OK, st_table_new(table_pool, (st_str_t)st_str_const("test"), &table), "");

    for (int i = 1; i < 10; i++) {
        st_ut_eq(ST_OK, st_table_get_ref(table_pool, (st_str_t)st_str_const("test"), &t), "");
    }

    st_refcnt_get_total_refcnt(&table->refcnt, &cnt);
    st_ut_eq(10, cnt, "");

    for (int i = 1; i < 9; i++) {
        st_ut_eq(ST_OK, st_table_return_ref(t), "");

        st_refcnt_get_total_refcnt(&table->refcnt, &cnt);
        st_ut_eq(10 - i, cnt, "");
    }

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_ut_eq(ST_OK, st_table_release(table), "");
    st_refcnt_get_total_refcnt(&table->refcnt, &cnt);
    st_ut_eq(1, cnt, "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 0, 1);

    st_ut_eq(ST_OK, st_table_return_ref(t), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    st_ut_eq(ST_ARG_INVALID, st_table_return_ref(NULL), "");

    free_table_pool(table_pool, 1);
}

st_test(table, release) {

    st_table_t *t;
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + strlen("test") + sizeof(t);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t);


    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_ut_eq(ST_OK, st_table_release(t), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    free_table_pool(table_pool, 1);
}

st_test(table, release_not_same_ref_table) {

    st_table_t *t1, *t2;
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + strlen("test") + sizeof(t1);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t1);
    st_table_get_ref(table_pool, (st_str_t)st_str_const("test"), &t1);

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    st_ut_eq(ST_OK, st_table_release(t1), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 0, 1);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t2);

    assure_alloc_struct_num_from_slab(table_pool, element_size, 2, 1, 2);

    st_ut_eq(ST_NOT_EQUAL, st_table_release(t1), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 2, 1, 2);

    st_ut_eq(ST_OK, st_table_release(t2), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 0, 1);

    st_ut_eq(ST_OK, st_table_return_ref(t1), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    free_table_pool(table_pool, 1);
}

st_test(table, insert_value) {

    st_table_t *t;
    st_str_t found;
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + 9 + sizeof(int);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t);

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    char buf[50];

    for (int i = 0; i < 100; i++) {
        sprintf(buf, "%03d%03d%03d", i, i + 1, i + 2);
        st_str_t key = st_str_wrap(buf, 9);
        st_str_t val = st_str_wrap(&i, sizeof(i));

        st_ut_eq(ST_OK, st_table_insert_value(t, key, val, 0, 0), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 1, i + 2, 1);

        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &val), "");

        st_ut_eq(ST_EXISTED, st_table_insert_value(t, key, val, 0, 0), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 1, i + 2, 1);

        st_ut_eq(ST_OK, st_table_insert_value(t, key, val, 0, 1), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 1, i + 2, 1);

        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &val), "");
    }

    st_str_t key = st_str_const("aa");
    st_str_t val = st_str_const("bb");

    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(NULL, key, val, 0, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, (st_str_t)st_str_null, val, 0, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, key, (st_str_t)st_str_null, 0, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, key, val, 2, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, key, val, 0, 2), "");

    free_table_pool(table_pool, 1);
}

st_test(table, remove_value) {

    st_table_t *t;
    st_str_t found;
    char buf[50];
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + 9 + sizeof(int);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t);

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 1, 1);

    for (int i = 0; i < 100; i++) {
        sprintf(buf, "%03d%03d%03d", i, i + 1, i + 2);
        st_str_t key = st_str_wrap(buf, 9);
        st_str_t val = st_str_wrap(&i, sizeof(i));

        st_table_insert_value(t, key, val, 0, 0);
        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 1, i + 2, 1);
    }

    for (int i = 0; i < 100; i++) {
        sprintf(buf, "%03d%03d%03d", i, i + 1, i + 2);
        st_str_t key = st_str_wrap(buf, 9);
        st_ut_eq(ST_OK, st_table_remove_value(t, key), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 1, 100 - i, 1);

        st_ut_eq(ST_NOT_FOUND, st_table_get_value(t, key, &found), "");
    }

    st_ut_eq(ST_NOT_FOUND, st_table_remove_value(t, (st_str_t)st_str_const("val")), "");

    st_ut_eq(ST_ARG_INVALID, st_table_remove_value(NULL, (st_str_t)st_str_const("val")), "");
    st_ut_eq(ST_ARG_INVALID, st_table_remove_value(t, (st_str_t)st_str_null), "");

    free_table_pool(table_pool, 1);
}

st_test(table, insert_remove_table) {

    int64_t cnt;
    st_table_t *table, *t1, *t2;
    st_str_t found;

    int element_size = sizeof(st_table_element_t) + 9 + sizeof(int);
    st_table_pool_t *table_pool = alloc_table_pool();

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &table);
    st_table_new(table_pool, (st_str_t)st_str_const("test1"), &t1);
    st_table_new(table_pool, (st_str_t)st_str_const("test2"), &t2);

    assure_alloc_struct_num_from_slab(table_pool, element_size, 3, 3, 3);

    char buf[50];

    for (int i = 0; i < 100; i++) {
        sprintf(buf, "%03d%03d%03d", i, i + 1, i + 2);
        st_str_t key = st_str_wrap(buf, 9);
        st_str_t val1 = st_str_wrap(&t1, sizeof(t1));

        st_ut_eq(ST_OK, st_table_insert_value(table, key, val1, 1, 0), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 3, i + 4, 3);

        st_ut_eq(ST_OK, st_table_get_value(table, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &val1), "");

        st_refcnt_get_total_refcnt(&t1->refcnt, &cnt);
        st_ut_eq(2, cnt, "");

        st_str_t val2 = st_str_wrap(&t2, sizeof(t2));
        st_ut_eq(ST_EXISTED, st_table_insert_value(table, key, val2, 1, 0), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 3, i + 4, 3);

        st_ut_eq(ST_OK, st_table_insert_value(table, key, val2, 1, 1), "");

        assure_alloc_struct_num_from_slab(table_pool, element_size, 3, i + 4, 3);

        st_ut_eq(ST_OK, st_table_get_value(table, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &val2), "");

        st_refcnt_get_total_refcnt(&t2->refcnt, &cnt);
        st_ut_eq(i + 2, cnt, "");
    }

    for (int i = 0; i < 100; i++) {
        sprintf(buf, "%03d%03d%03d", i, i + 1, i + 2);
        st_str_t key = st_str_wrap(buf, 9);

        assure_alloc_struct_num_from_slab(table_pool, element_size, 3, 103 - i, 3);

        st_table_remove_value(table, key);

        assure_alloc_struct_num_from_slab(table_pool, element_size, 3, 102 - i, 3);

        st_refcnt_get_total_refcnt(&t2->refcnt, &cnt);
        st_ut_eq(100 - i, cnt, "");
    }

    st_refcnt_get_total_refcnt(&t2->refcnt, &cnt);
    st_ut_eq(1, cnt, "");

    st_refcnt_get_total_refcnt(&t1->refcnt, &cnt);
    st_ut_eq(1, cnt, "");

    free_table_pool(table_pool, 1);
}

st_test(table, get_all_elements) {

    int i;
    st_table_t *t;
    st_table_pool_t *table_pool = alloc_table_pool();
    st_list_t all = ST_LIST_INIT(all);

    st_table_new(table_pool, (st_str_t)st_str_const("test"), &t);

    for (i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_str_t val = st_str_wrap(&i, sizeof(i));
        st_table_insert_value(t, key, val, 0, 0);
    }

    st_ut_eq(ST_OK, st_table_lock(t), "");

    st_ut_eq(ST_OK, st_table_get_all_elements(t, &all), "");

    st_table_element_t *e = NULL;

    i = 0;
    st_list_for_each_entry(e, &all, lnode) {
        st_ut_eq(i, *(int *)(e->key.bytes), "");
        st_ut_eq(i, *(int *)(e->val.bytes), "");
        i++;
    }

    st_ut_eq(ST_OK, st_table_unlock(t), "");

    free_table_pool(table_pool, 1);
}

st_test(table, clean_process_ref) {

    int64_t cnt;
    char buf[50];
    st_table_t *t;
    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + 9 + sizeof(t);

    for (int i = 0; i < 100; i++) {
        sprintf(buf, "%03d%03d%03d", i, i + 1, i + 2);
        st_str_t name = st_str_wrap(buf, 9);

        st_table_new(table_pool, name, &t);

        for (int j = 0; j < 10; j++) {
            st_ut_eq(ST_OK, st_table_get_ref(table_pool, name, &t), "");
        }

        st_refcnt_get_total_refcnt(&t->refcnt, &cnt);
        st_ut_eq(11, cnt, "");
    }

    st_ut_eq(0, st_rbtree_is_empty(&table_pool->root_table.elements), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 100, 100, 100);

    st_ut_eq(ST_OK, st_table_clean_process_ref(table_pool, getpid()), "");

    st_ut_eq(1, st_rbtree_is_empty(&table_pool->root_table.elements), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    free_table_pool(table_pool, 1);
}

int block_all_children(int sem_id) {
    union semun sem_args;
    sem_args.val = 0;

    int ret = semctl(sem_id, 0, SETVAL, sem_args);
    if (ret == -1) {
        return ST_ERR;
    }

    return ST_OK;
}

int wakeup_all_children(int sem_id, int children_num) {
    struct sembuf sem = {.sem_num = 0, .sem_op = children_num, .sem_flg = SEM_UNDO};

    int ret = semop(sem_id, &sem, 1);
    if (ret == -1) {
        return ST_ERR;
    }

    return ST_OK;
}

int wait_sem(int sem_id) {
    struct sembuf sem = {.sem_num = 0, .sem_op = -1, .sem_flg = SEM_UNDO};

    int ret = semop(sem_id, &sem, 1);
    if (ret == -1) {
        return ST_ERR;
    }

    return ST_OK;
}

static int run_processes(int sem_id, process_f func, st_table_pool_t *table_pool, int *pids,
                         int process_num) {

    int child;

    int ret = block_all_children(sem_id);
    if (ret == ST_ERR) {
        return ret;
    }

    for (int i = 0; i < process_num; i++) {

        child = fork();

        if (child == 0) {

            set_process_to_cpu(i);

            ret = wait_sem(sem_id);
            if (ret == ST_ERR) {
                exit(ret);
            }

            ret = func(i, table_pool);

            exit(ret);
        }

        pids[i] = child;
    }

    sleep(2);

    ret = wakeup_all_children(sem_id, process_num);
    if (ret != ST_OK) {
        return ret;
    }

    return wait_children(pids, process_num);
}

static int new_tables(int process_id, st_table_pool_t *table_pool) {

    int ret;
    st_table_t *t;
    char buf[50];

    for (int i = 0; i < 100; i++) {

        sprintf(buf, "%03d%03d", process_id, i);
        st_str_t name = st_str_wrap(buf, 6);

        ret = st_table_new(table_pool, name, &t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return ST_OK;
}

static int insert_tables(int process_id, st_table_pool_t *table_pool) {

    int ret;
    st_table_t *process_tables[10];
    char buf[50];
    st_table_t *t;

    for (int i = 0; i < 10; i++) {
        sprintf(buf, "%03d000", i);
        st_str_t name = st_str_wrap(buf, 6);

        ret = st_table_get_ref(table_pool, name, &process_tables[i]);
        if (ret != ST_OK) {
            return ret;
        }
    }

    for (int i = 0; i < 10; i++) {

        for (int j = 1; j < 100; j++) {

            sprintf(buf, "%03d%03d", i, j);
            st_str_t name = st_str_wrap(buf, 6);

            ret = st_table_get_ref(table_pool, name, &t);
            if (ret != ST_OK) {
                return ret;
            }

            sprintf(buf, "%03d%03d%03d", process_id, i, j);
            st_str_t key = st_str_wrap(buf, 9);

            st_str_t val = st_str_wrap(&t, sizeof(t));

            for (int k = 0; k < 10; k++) {
                ret = st_table_insert_value(process_tables[k], key, val, 1, 0);
                if (ret != ST_OK) {
                    return ret;
                }
            }

            ret = st_table_return_ref(t);
            if (ret != ST_OK) {
                return ret;
            }
        }
    }

    for (int i = 0; i < 10; i++) {
        for (int j = 1; j < 100; j++) {

            sprintf(buf, "%03d%03d%03d", process_id, i, j);
            st_str_t key = st_str_wrap(buf, 9);

            for (int k = 0; k < 10; k++) {
                ret = st_table_remove_value(process_tables[k], key);
                if (ret != ST_OK) {
                    return ret;
                }
            }
        }
    }

    for (int i = 0; i < 10; i++) {
        ret = st_table_return_ref(process_tables[i]);
        if (ret != ST_OK) {
            return ret;
        }
    }
    return ST_OK;
}

st_test(table, handle_table_in_processes) {

    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + 6 + sizeof(st_table_t *);

    int sem_id = semget(5678, 1, 06666 | IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    int new_table_pids[10];
    int insert_table_pids[10];

    st_ut_eq(ST_OK, run_processes(sem_id, new_tables, table_pool, new_table_pids, 10), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1000, 1000, 1000);

    st_ut_eq(ST_OK, run_processes(sem_id, insert_tables, table_pool, insert_table_pids, 10), "");

    assure_alloc_struct_num_from_slab(table_pool, element_size, 1000, 1000, 1000);

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_table_clean_process_ref(table_pool, new_table_pids[i]), "");
        st_ut_eq(ST_OK, st_table_clean_process_ref(table_pool, insert_table_pids[i]), "");
    }

    assure_alloc_struct_num_from_slab(table_pool, element_size, 0, 0, 0);

    semctl(sem_id, 0, IPC_RMID);
    free_table_pool(table_pool, 1);
}

st_ut_main;
