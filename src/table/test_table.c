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

typedef int (*process_f)(int process_id, st_table_t *root);

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

static ssize_t get_alloc_num_in_slab(st_slab_pool_t *slab_pool, uint64_t size) {

    int idx = _slab_size_to_index(size);

    return slab_pool->groups[idx].stat.current.alloc.cnt;
}

static st_slab_pool_t *alloc_slab_pool() {
    st_slab_pool_t *slab_pool = mmap(NULL, TEST_POOL_SIZE, PROT_READ | PROT_WRITE,
                                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    st_assert(slab_pool != MAP_FAILED);

    memset(slab_pool, 0, sizeof(st_slab_pool_t));

    void *data = (void *)(st_align((uintptr_t)slab_pool + sizeof(st_slab_pool_t), 4096));

    int region_size = 1024 * 4096;
    int ret = st_region_init(&slab_pool->page_pool.region_cb, data,
                             region_size / 4096, TEST_POOL_SIZE / region_size, 0);
    st_assert(ret == ST_OK);

    ret = st_pagepool_init(&slab_pool->page_pool, 4096);
    st_assert(ret == ST_OK);

    ret = st_slab_pool_init(slab_pool);
    st_assert(ret == ST_OK);

    return slab_pool;
}

static void free_slab_pool(st_slab_pool_t *slab_pool) {

    int ret = st_slab_pool_destroy(slab_pool);
    st_assert(ret == ST_OK);

    ret = st_pagepool_destroy(&slab_pool->page_pool);
    st_assert(ret == ST_OK);

    ret = st_region_destroy(&slab_pool->page_pool.region_cb);
    st_assert(ret == ST_OK);

    munmap(slab_pool, TEST_POOL_SIZE);
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

st_test(table, new_release) {

    st_table_t *t;
    st_slab_pool_t *slab_pool = alloc_slab_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) * 2;

    st_ut_eq(ST_OK, st_table_new(slab_pool, &t), "");

    st_ut_eq(1, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    st_ut_eq(1, st_rbtree_is_empty(&t->elements), "");
    st_ut_eq(t->slab_pool, slab_pool, "");
    st_ut_eq(1, t->refcnt, "");
    st_ut_eq(1, t->inited, "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_str_t value = st_str_wrap(&i, sizeof(i));

        st_ut_eq(ST_OK, st_table_insert_value(t, key, value, 0), "");
        st_ut_eq(i + 1, get_alloc_num_in_slab(slab_pool, element_size), "");
    }

    st_ut_eq(100, get_alloc_num_in_slab(slab_pool, element_size), "");

    st_ut_eq(ST_OK, st_table_release(t), "");

    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");

    st_ut_eq(ST_ARG_INVALID, st_table_new(NULL, &t), "");
    st_ut_eq(ST_ARG_INVALID, st_table_new(slab_pool, NULL), "");

    st_ut_eq(ST_ARG_INVALID, st_table_release(NULL), "");

    free_slab_pool(slab_pool);
}

st_test(table, get_return_reference) {

    st_slab_pool_t *slab_pool = alloc_slab_pool();

    st_table_t *t;

    st_table_new(slab_pool, &t);

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_table_get_reference(t), "");
        st_ut_eq(i + 2, t->refcnt, "");
    }

    st_ut_eq(1, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    for (int i = 0; i < 10; i++) {
        st_ut_eq(ST_OK, st_table_return_reference(t), "");
        st_ut_eq(10 - i, t->refcnt, "");
    }

    st_ut_eq(1, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    st_ut_eq(ST_OK, st_table_release(t), "");

    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    st_ut_eq(ST_ARG_INVALID, st_table_get_reference(NULL), "");
    st_ut_eq(ST_ARG_INVALID, st_table_return_reference(NULL), "");

    free_slab_pool(slab_pool);
}

st_test(table, insert_value) {

    st_table_t *t;
    st_str_t found;
    st_slab_pool_t *slab_pool = alloc_slab_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) * 2;

    st_table_new(slab_pool, &t);

    st_ut_eq(1, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_str_t value = st_str_wrap(&i, sizeof(i));

        st_ut_eq(ST_OK, st_table_insert_value(t, key, value, 0), "");

        st_ut_eq(i + 1, get_alloc_num_in_slab(slab_pool, element_size), "");

        st_ut_eq(ST_OK, st_table_get_value(t, key, &found), "");
        st_ut_eq(0, st_str_cmp(&found, &value), "");

        st_ut_eq(ST_EXISTED, st_table_insert_value(t, key, value, 0), "");

        st_ut_eq(i + 1, get_alloc_num_in_slab(slab_pool, element_size), "");
    }

    st_ut_eq(1, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(100, get_alloc_num_in_slab(slab_pool, element_size), "");

    st_str_t key = st_str_const("aa");
    st_str_t val = st_str_const("bb");

    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(NULL, key, val, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, (st_str_t)st_str_null, val, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, key, (st_str_t)st_str_null, 0), "");
    st_ut_eq(ST_ARG_INVALID, st_table_insert_value(t, key, val, 2), "");

    st_table_release(t);
    free_slab_pool(slab_pool);
}

st_test(table, remove_value) {

    st_table_t *t;
    st_str_t found;
    st_slab_pool_t *slab_pool = alloc_slab_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) * 2;

    st_table_new(slab_pool, &t);

    st_ut_eq(1, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_str_t value = st_str_wrap(&i, sizeof(i));

        st_table_insert_value(t, key, value, 0);
    }

    st_ut_eq(100, get_alloc_num_in_slab(slab_pool, element_size), "");

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_ut_eq(ST_OK, st_table_remove_value(t, key), "");

        st_ut_eq(99 - i, get_alloc_num_in_slab(slab_pool, element_size), "");

        st_ut_eq(ST_NOT_FOUND, st_table_get_value(t, key, &found), "");
    }

    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");

    st_ut_eq(ST_ARG_INVALID, st_table_remove_value(NULL, (st_str_t)st_str_const("val")), "");
    st_ut_eq(ST_ARG_INVALID, st_table_remove_value(t, (st_str_t)st_str_null), "");

    st_table_release(t);
    free_slab_pool(slab_pool);
}

st_test(table, insert_remove_table) {

    st_table_t *table, *t1;

    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(st_table_t *);
    st_slab_pool_t *slab_pool = alloc_slab_pool();


    st_table_new(slab_pool, &table);
    st_table_new(slab_pool, &t1);

    st_ut_eq(2, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");


    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_str_t val1 = st_str_wrap(&t1, sizeof(t1));

        st_ut_eq(ST_OK, st_table_insert_value(table, key, val1, 1), "");

        st_ut_eq(1, table->refcnt, "");
        st_ut_eq(i + 2, t1->refcnt, "");

        st_ut_eq(2, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
        st_ut_eq(i + 1, get_alloc_num_in_slab(slab_pool, element_size), "");
    }

    for (int i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        st_table_remove_value(table, key);

        st_ut_eq(1, table->refcnt, "");
        st_ut_eq(100 - i, t1->refcnt, "");

        st_ut_eq(2, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
        st_ut_eq(99 - i, get_alloc_num_in_slab(slab_pool, element_size), "");
    }

    st_table_release(table);
    st_table_release(t1);
    free_slab_pool(slab_pool);
}

st_test(table, get_all_elements) {

    int i;
    st_table_t *t;
    st_slab_pool_t *slab_pool = alloc_slab_pool();
    st_list_t all = ST_LIST_INIT(all);

    st_table_new(slab_pool, &t);

    for (i = 0; i < 100; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));
        st_str_t value = st_str_wrap(&i, sizeof(i));
        st_table_insert_value(t, key, value, 0);
    }

    st_ut_eq(ST_OK, st_table_lock(t), "");

    st_ut_eq(ST_OK, st_table_get_all_sorted_elements(t, &all), "");

    st_table_element_t *e = NULL;

    i = 0;
    st_list_for_each_entry(e, &all, lnode) {
        st_ut_eq(i, *(int *)(e->key.bytes), "");
        st_ut_eq(i, *(int *)(e->value.bytes), "");
        i++;
    }

    st_ut_eq(ST_OK, st_table_unlock(t), "");

    st_table_release(t);
    free_slab_pool(slab_pool);
}

st_test(table, get_table_with_reference) {

    st_table_t *table, *t, *sub;
    int element_size = sizeof(st_table_element_t) + 2 + sizeof(st_table_t *);
    st_slab_pool_t *slab_pool = alloc_slab_pool();

    st_table_new(slab_pool, &table);
    st_table_new(slab_pool, &t);

    st_str_t key = st_str_const("aa");
    st_str_t val = st_str_wrap(&t, sizeof(t));
    st_table_insert_value(table, key, val, 1);

    for (int i = 0; i < 100; i++) {
        st_ut_eq(ST_OK, st_table_get_table_with_reference(table, key, &sub), "");
        st_ut_eq(t, sub, "");
        st_ut_eq(i + 3, sub->refcnt, "");
    }

    for (int i = 0; i < 100; i++) {
        st_table_return_reference(t);
    }

    st_table_remove_value(table, key);

    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");

    st_ut_eq(2, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    st_table_release(table);
    st_table_release(t);

    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    free_slab_pool(slab_pool);
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

static int run_processes(int sem_id, process_f func, st_table_t *root, int *pids,
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

            ret = func(i, root);

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

static int new_tables(int process_id, st_table_t *root) {

    int ret, tmp;
    st_table_t *t;
    st_slab_pool_t *slab_pool = root->slab_pool;

    for (int i = 0; i < 100; i++) {

        ret = st_table_new(slab_pool, &t);
        if (ret != ST_OK) {
            return ret;
        }

        tmp = process_id * 100 + i;
        st_str_t key = st_str_wrap(&tmp, sizeof(tmp));
        st_str_t val = st_str_wrap(&t, sizeof(t));

        ret = st_table_insert_value(root, key, val, 1);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_table_release(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return ST_OK;
}

static int insert_tables(int process_id, st_table_t *root) {

    int ret, tmp;
    st_table_t *t;
    st_table_t *process_tables[10];

    for (int i = 0; i < 10; i++) {
        tmp = process_id * 100 + i;
        st_str_t key = st_str_wrap(&tmp, sizeof(tmp));

        ret = st_table_get_table_with_reference(root, key, &process_tables[i]);
        if (ret != ST_OK) {
            return ret;
        }
    }

    for (int i = 10; i < 1000; i++) {
        st_str_t key = st_str_wrap(&i, sizeof(i));

        ret = st_table_get_table_with_reference(root, key, &t);
        if (ret != ST_OK) {
            return ret;
        }

        tmp = process_id * 1000 + i;
        key = (st_str_t)st_str_wrap(&tmp, sizeof(tmp));
        st_str_t val = st_str_wrap(&t, sizeof(t));

        for (int j = 0; j < 10; j++) {
            ret = st_table_insert_value(process_tables[j], key, val, 1);
            if (ret != ST_OK) {
                return ret;
            }
        }

        ret = st_table_return_reference(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    for (int i = 10; i < 1000; i++) {
        tmp = process_id * 1000 + i;
        st_str_t key = st_str_wrap(&tmp, sizeof(tmp));

        for (int j = 0; j < 10; j++) {
            ret = st_table_remove_value(process_tables[j], key);
            if (ret != ST_OK) {
                return ret;
            }
        }
    }

    for (int i = 0; i < 10; i++) {
        ret = st_table_return_reference(process_tables[i]);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return ST_OK;
}

st_test(table, handle_table_in_processes) {

    st_slab_pool_t *slab_pool = alloc_slab_pool();
    int element_size = sizeof(st_table_element_t) + sizeof(int) + sizeof(st_table_t *);

    int sem_id = semget(5678, 1, 06666 | IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    int new_table_pids[10];
    int insert_table_pids[10];

    st_table_t *root;
    st_table_new(slab_pool, &root);

    st_ut_eq(ST_OK, run_processes(sem_id, new_tables, root, new_table_pids, 10), "");

    st_ut_eq(1001, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");

    st_ut_eq(ST_OK, run_processes(sem_id, insert_tables, root, insert_table_pids, 10), "");

    st_ut_eq(1001, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(1000, get_alloc_num_in_slab(slab_pool, element_size), "");

    st_ut_eq(ST_OK, st_table_release(root), "");

    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, sizeof(st_table_t)), "");
    st_ut_eq(0, get_alloc_num_in_slab(slab_pool, element_size), "");

    semctl(sem_id, 0, IPC_RMID);
    free_slab_pool(slab_pool);
}

st_ut_main;
