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

typedef int (*process_f)(int process_id, void *arg);

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

static ssize_t remain_element_cnt(st_table_pool_t *pool, uint64_t element_size) {
    return get_alloc_cnt_in_slab(pool, element_size);
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

static void run_gc_one_round(st_gc_t *gc) {
    int ret;

    do {
        ret = st_gc_run(gc);
        st_assert(ret == ST_OK || ret == ST_NO_GC_DATA);
    } while (gc->begin);
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

st_test(table, new_free) {

    st_table_t *t;
    st_table_pool_t *pool = alloc_table_pool();

    // use big value buffer, because want element size different from slab chunk size
    // from table chunk size.
    int value_buf[40] = {0};

    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(value_buf);

    st_ut_eq(ST_OK, st_table_new(pool, &t), "");

    st_ut_eq(1, remain_table_cnt(pool), "");

    st_ut_eq(1, st_rbtree_is_empty(&t->elements), "");
    st_ut_eq(t->pool, pool, "");
    st_ut_eq(1, t->inited, "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        value_buf[0] = i;
        st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));

        st_ut_eq(ST_OK, st_table_add_key_value(t, key, value), "");
        st_ut_eq(i + 1, remain_element_cnt(pool, element_size), "");
    }

    st_ut_eq(100, remain_element_cnt(pool, element_size), "");

    st_ut_eq(ST_OK, st_table_remove_all(t), "");
    st_ut_eq(ST_OK, st_table_free(t), "");

    st_ut_eq(0, remain_table_cnt(pool), "");
    st_ut_eq(0, remain_element_cnt(pool, element_size), "");

    st_ut_eq(ST_ARG_INVALID, st_table_new(NULL, &t), "");
    st_ut_eq(ST_ARG_INVALID, st_table_new(pool, NULL), "");

    st_ut_eq(ST_ARG_INVALID, st_table_free(NULL), "");

    free_table_pool(pool);
}

st_test(table, add_key_value) {

    st_table_t *t;
    st_str_t found;
    int value_buf[40] = {0};

    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(value_buf);

    st_table_new(table_pool, &t);

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        value_buf[0] = i;
        st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));

        st_ut_eq(ST_OK, st_table_add_key_value(t, key, value), "");

        st_ut_eq(i + 1, remain_element_cnt(table_pool, element_size), "");

        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &value), "");

        st_ut_eq(ST_EXISTED, st_table_add_key_value(t, key, value), "");

        st_ut_eq(i + 1, remain_element_cnt(table_pool, element_size), "");

        st_ut_eq(i + 1, t->element_cnt, "");
    }

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(100, remain_element_cnt(table_pool, element_size), "");

    st_str_t key = st_str_const("aa");
    st_str_t value = st_str_const("bb");

    st_ut_eq(ST_ARG_INVALID, st_table_add_key_value(NULL, key, value), "");
    st_ut_eq(ST_ARG_INVALID, st_table_add_key_value(t, (st_str_t)st_str_null, value), "");
    st_ut_eq(ST_ARG_INVALID, st_table_add_key_value(t, key, (st_str_t)st_str_null), "");

    st_table_remove_all(t);
    st_table_free(t);
    free_table_pool(table_pool);
}

st_test(table, set_key_value) {

    st_table_t *t;
    st_str_t found;
    int value_buf[40] = {0};

    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(value_buf);

    st_table_new(table_pool, &t);

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        value_buf[0] = i;
        st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));
        st_ut_eq(ST_OK, st_table_set_key_value(t, key, value), "");

        st_ut_eq(i + 1, remain_element_cnt(table_pool, element_size), "");

        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &value), "");

        value_buf[0] = i + 1000;
        value = (st_str_t)st_str_wrap(value_buf, sizeof(value_buf));

        st_ut_eq(ST_OK, st_table_set_key_value(t, key, value), "");
        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");

        st_ut_eq(0, st_str_cmp(&found, &value), "");

        st_ut_eq(i + 1, remain_element_cnt(table_pool, element_size), "");

        st_ut_eq(i + 1, t->element_cnt, "");
    }

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(100, remain_element_cnt(table_pool, element_size), "");

    st_str_t key = st_str_const("aa");
    st_str_t value = st_str_const("bb");

    st_ut_eq(ST_ARG_INVALID, st_table_set_key_value(NULL, key, value), "");
    st_ut_eq(ST_ARG_INVALID, st_table_set_key_value(t, (st_str_t)st_str_null, value), "");
    st_ut_eq(ST_ARG_INVALID, st_table_set_key_value(t, key, (st_str_t)st_str_null), "");

    st_table_remove_all(t);
    st_table_free(t);
    free_table_pool(table_pool);
}

st_test(table, remove_all) {

    st_table_t *t;
    int value_buf[40] = {0};

    st_table_pool_t *table_pool = alloc_table_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(value_buf);

    st_table_new(table_pool, &t);

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        value_buf[0] = i;
        st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));
        st_ut_eq(ST_OK, st_table_add_key_value(t, key, value), "");

        st_ut_eq(i + 1, remain_element_cnt(table_pool, element_size), "");
        st_ut_eq(i + 1, t->element_cnt, "");
    }

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(100, remain_element_cnt(table_pool, element_size), "");

    st_ut_eq(ST_OK, st_table_remove_all(t), "");

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    st_table_free(t);
    free_table_pool(table_pool);
}

st_test(table, remove_key) {

    st_table_t *t;
    st_str_t found;
    int value_buf[40] = {0};

    st_table_pool_t *table_pool = alloc_table_pool();

    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(value_buf);

    st_table_new(table_pool, &t);

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        value_buf[0] = i;
        st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));

        st_table_add_key_value(t, key, value);
    }

    st_ut_eq(100, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        st_ut_eq(ST_OK, st_table_remove_key(t, key), "");

        st_ut_eq(99 - i, remain_element_cnt(table_pool, element_size), "");
        st_ut_eq(99 - i, t->element_cnt, "");

        st_ut_eq(ST_NOT_FOUND, st_table_get_value(t, key, &found), "");
    }

    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    st_ut_eq(ST_ARG_INVALID, st_table_remove_key(NULL, (st_str_t)st_str_const("val")), "");
    st_ut_eq(ST_ARG_INVALID, st_table_remove_key(t, (st_str_t)st_str_null), "");

    st_table_remove_all(t);
    st_table_free(t);
    free_table_pool(table_pool);
}

st_test(table, iter_next_key_value) {

    int i;
    st_table_t *t;
    int value_buf[40] = {0};

    st_table_pool_t *table_pool = alloc_table_pool();
    st_list_t all = ST_LIST_INIT(all);

    st_table_new(table_pool, &t);

    for (i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        value_buf[0] = i;
        st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));

        st_table_add_key_value(t, key, value);
    }

    st_str_t k;
    st_str_t v1;
    st_str_t v2;
    st_table_iter_t iter;
    st_table_iter_init(t, &iter);

    for (i = 0; i < 100; i++) {
        st_ut_eq(ST_OK, st_table_iter_next(t, &iter, &k, &v1), "");

        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_ut_eq(ST_OK, st_table_get_value(t, key, &v2), "");

        st_ut_eq(0, st_str_cmp(&k, &key), "");
        st_ut_eq(0, st_str_cmp(&v1, &v2), "");
    }

    st_ut_eq(ST_NOT_FOUND, st_table_iter_next(t, &iter, &k, &v1), "");

    st_str_t key = st_str_wrap(&i, sizeof(i));
    value_buf[0] = 100;
    st_str_t value = st_str_wrap(value_buf, sizeof(value_buf));
    st_ut_eq(0, st_table_add_key_value(t, key, value), "");

    st_ut_eq(ST_TABLE_MODIFIED, st_table_iter_next(t, &iter, &k, &v1), "");

    st_table_remove_all(t);
    st_table_free(t);
    free_table_pool(table_pool);
}

void add_sub_table(st_table_t *table, char *name, st_table_t *sub) {

    char key_buf[11] = {0};
    int value_buf[40] = {0};

    memcpy(key_buf, name, strlen(name));
    st_str_t key = st_str_wrap(key_buf, strlen(key_buf));

    memcpy(value_buf, &sub, (size_t)sizeof(sub));
    st_str_t value = st_str_wrap_common(value_buf, ST_TYPES_TABLE, sizeof(value_buf));

    st_assert(st_table_add_key_value(table, key, value) == ST_OK);
}

st_test(table, add_remove_table) {

    st_table_t *root, *table, *t;
    char key_buf[11] = {0};
    int value_buf[40] = {0};

    int element_size = sizeof(st_table_element_t) + sizeof(key_buf) + sizeof(value_buf);
    st_table_pool_t *table_pool = alloc_table_pool();

    st_table_new(table_pool, &root);
    st_ut_eq(st_gc_add_root(&table_pool->gc, &root->gc_head), ST_OK, "");

    st_table_new(table_pool, &table);
    add_sub_table(root, "test_table", table);

    st_ut_eq(2, remain_table_cnt(table_pool), "");
    st_ut_eq(1, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        sprintf(key_buf, "%010d", i);
        st_table_new(table_pool, &t);
        add_sub_table(table, key_buf, t);

        st_ut_eq(i + 3, remain_table_cnt(table_pool), "");
        st_ut_eq(i + 2, remain_element_cnt(table_pool, element_size), "");
    }

    for (int i = 0; i < 100; i++) {
        sprintf(key_buf, "%010d", i);
        st_str_t key = st_str_wrap(key_buf, strlen(key_buf));
        st_ut_eq(ST_OK, st_table_remove_key(table, key), "");

        run_gc_one_round(&table_pool->gc);
        run_gc_one_round(&table_pool->gc);

        st_ut_eq(101 - i, remain_table_cnt(table_pool), "");
        st_ut_eq(100 - i, remain_element_cnt(table_pool, element_size), "");
    }

    st_ut_eq(ST_OK, st_table_remove_all(root), "");
    run_gc_one_round(&table_pool->gc);

    st_ut_eq(ST_OK, st_gc_remove_root(&table_pool->gc, &root->gc_head), "");
    st_ut_eq(ST_OK, st_table_free(root), "");

    st_ut_eq(0, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    free_table_pool(table_pool);
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

int wakeup_all_children(int sem_id, int children_cnt) {
    struct sembuf sem = {.sem_num = 0, .sem_op = children_cnt, .sem_flg = SEM_UNDO};

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

static int run_processes(int sem_id, process_f func, void *arg, int *pids,
                         int process_cnt) {

    int child;

    int ret = block_all_children(sem_id);
    if (ret == ST_ERR) {
        return ret;
    }

    for (int i = 0; i < process_cnt; i++) {

        child = fork();

        if (child == 0) {

            set_process_to_cpu(i);

            ret = wait_sem(sem_id);
            if (ret == ST_ERR) {
                exit(ret);
            }

            ret = func(i, arg);
            exit(ret);
        }

        pids[i] = child;
    }

    sleep(2);

    ret = wakeup_all_children(sem_id, process_cnt);
    if (ret != ST_OK) {
        return ret;
    }

    return wait_children(pids, process_cnt);
}

static int new_tables(int process_id, st_table_t *root) {

    int ret;
    st_table_t *t;
    char key_buf[11] = {0};
    st_table_pool_t *table_pool = root->pool;

    for (int i = 0; i < 100; i++) {
        ret = st_table_new(table_pool, &t);
        if (ret != ST_OK) {
            return ret;
        }

        sprintf(key_buf, "%010d", process_id * 100 + i);
        add_sub_table(root, key_buf, t);
    }

    return ST_OK;
}

static int add_tables(int process_id, st_table_t *root) {

    int ret;
    st_str_t value;
    char key_buf[11] = {0};
    st_table_t *process_tables[10];

    for (int i = 0; i < 10; i++) {
        sprintf(key_buf, "%010d", process_id * 100 + i);
        st_str_t key = st_str_wrap(key_buf, strlen(key_buf));

        ret = st_table_get_value(root, key, &value);
        if (ret != ST_OK) {
            return ret;
        }

        process_tables[i] = st_table_get_table_addr_from_value(value);
    }

    for (int i = 10; i < 1000; i++) {
        sprintf(key_buf, "%010d", i);
        st_str_t key = st_str_wrap(key_buf, strlen(key_buf));

        ret = st_table_get_value(root, key, &value);
        if (ret != ST_OK) {
            return ret;
        }

        if (value.type != ST_TYPES_TABLE) {
            return ST_STATE_INVALID;
        }

        sprintf(key_buf, "%010d", process_id * 1000 + i);
        key = (st_str_t)st_str_wrap(key_buf, strlen(key_buf));

        for (int j = 0; j < 10; j++) {
            ret = st_table_add_key_value(process_tables[j], key, value);
            if (ret != ST_OK) {
                return ret;
            }
        }
    }

    for (int i = 10; i < 1000; i++) {
        sprintf(key_buf, "%010d", process_id * 1000 + i);
        st_str_t key = st_str_wrap(key_buf, strlen(key_buf));

        for (int j = 0; j < 10; j++) {
            ret = st_table_remove_key(process_tables[j], key);
            if (ret != ST_OK) {
                return ret;
            }
        }
    }

    run_gc_one_round(&root->pool->gc);

    return ST_OK;
}

st_test(table, test_table_in_processes) {

    st_table_pool_t *table_pool = alloc_table_pool();
    int value_buf[40] = {0};
    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(value_buf);

    int sem_id = semget(5678, 1, 06666 | IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    int new_table_pids[10];
    int add_table_pids[10];

    st_table_t *root;
    st_table_new(table_pool, &root);
    st_ut_eq(st_gc_add_root(&table_pool->gc, &root->gc_head), ST_OK, "");

    st_ut_eq(ST_OK, run_processes(sem_id, (process_f)new_tables, root, new_table_pids, 10), "");

    st_ut_eq(1001, remain_table_cnt(table_pool), "");

    st_ut_eq(ST_OK, run_processes(sem_id, (process_f)add_tables, root, add_table_pids, 10), "");

    st_ut_eq(1001, remain_table_cnt(table_pool), "");
    st_ut_eq(1000, remain_element_cnt(table_pool, element_size), "");

    st_ut_eq(ST_OK, st_table_remove_all(root), "");
    run_gc_one_round(&table_pool->gc);

    st_ut_eq(st_gc_remove_root(&table_pool->gc, &root->gc_head), ST_OK, "");
    st_ut_eq(ST_OK, st_table_free(root), "");

    st_ut_eq(0, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    semctl(sem_id, 0, IPC_RMID);
    free_table_pool(table_pool);
}

st_test(table, clear_circular_ref_in_same_table) {

    st_table_t *root, *t;
    char key_buf[11] = {0};
    int value_buf[40] = {0};

    int element_size = sizeof(st_table_element_t) + sizeof(key_buf) + sizeof(value_buf);
    st_table_pool_t *table_pool = alloc_table_pool();

    st_table_new(table_pool, &root);
    st_ut_eq(st_gc_add_root(&table_pool->gc, &root->gc_head), ST_OK, "");

    st_table_new(table_pool, &t);
    add_sub_table(root, "test_table", t);

    st_ut_eq(2, remain_table_cnt(table_pool), "");
    st_ut_eq(1, remain_element_cnt(table_pool, element_size), "");

    for (int i = 0; i < 100; i++) {

        sprintf(key_buf, "%010d", i);
        add_sub_table(t, key_buf, t);

        st_ut_eq(i + 2, remain_element_cnt(table_pool, element_size), "");
    }

    st_ut_eq(2, remain_table_cnt(table_pool), "");
    st_ut_eq(101, remain_element_cnt(table_pool, element_size), "");

    st_ut_eq(ST_OK, st_table_remove_all(root), "");

    run_gc_one_round(&table_pool->gc);
    run_gc_one_round(&table_pool->gc);

    st_ut_eq(1, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    st_ut_eq(st_gc_remove_root(&table_pool->gc, &root->gc_head), ST_OK, "");
    st_ut_eq(ST_OK, st_table_free(root), "");

    st_ut_eq(0, remain_table_cnt(table_pool), "");

    free_table_pool(table_pool);
}

static int test_circular_ref(int process_id, st_table_pool_t *table_pool) {

    int ret;
    st_table_t *root, *t1, *t2;

    srand(process_id);
    int r = random() % 50 + 50;

    char key_buf[10];

    st_table_new(table_pool, &root);
    st_ut_eq(st_gc_add_root(&table_pool->gc, &root->gc_head), ST_OK, "");

    for (int i = 0; i < 10000; i++) {

        ret = st_table_new(table_pool, &t1);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_table_new(table_pool, &t2);
        if (ret != ST_OK) {
            return ret;
        }

        sprintf(key_buf, "t1%08d", i);

        add_sub_table(root, key_buf, t1);

        sprintf(key_buf, "t2%08d", i);
        add_sub_table(root, key_buf, t2);

        add_sub_table(t1, "sub_t2", t2);
        add_sub_table(t2, "sub_t1", t1);

        if (i % r == 0) {
            ret = st_table_remove_all(root);
            if (ret != ST_OK) {
                return ret;
            }
        }
    }

    ret = st_table_remove_all(root);
    if (ret != ST_OK) {
        return ret;
    }

    while (1) {
        int ret = st_gc_run(&table_pool->gc);
        if (ret == ST_NO_GC_DATA) {
            break;
        }

        if (ret != ST_OK) {
            return ret;
        }
    }

    ret = st_gc_remove_root(&table_pool->gc, &root->gc_head);
    if (ret != ST_OK) {
        return ret;
    }

    return st_table_free(root);
}

st_test(table, test_circular_ref) {

    st_table_pool_t *table_pool = alloc_table_pool();
    char key_buf[11] = {0};
    int value_buf[40] = {0};
    int element_size = sizeof(st_table_element_t) + sizeof(key_buf) + sizeof(value_buf);

    int sem_id = semget(6789, 1, 06666 | IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    int pids[10];

    st_ut_eq(ST_OK, run_processes(sem_id, (process_f)test_circular_ref, table_pool, pids, 10), "");

    st_ut_eq(0, remain_table_cnt(table_pool), "");
    st_ut_eq(0, remain_element_cnt(table_pool, element_size), "");

    semctl(sem_id, 0, IPC_RMID);
    free_table_pool(table_pool);
}

st_ut_main;
