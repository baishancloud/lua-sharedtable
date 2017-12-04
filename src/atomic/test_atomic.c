#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#include <sys/sem.h>
#include <sys/ipc.h>
#include <time.h>
#include <stdlib.h>
#include <sched.h>

#include "atomic.h"
#include "inc/err.h"
#include "unittest/unittest.h"

#define ST_ATOMIC_FETCH_ADD  0
#define ST_ATOMIC_FETCH_SUB  1
#define ST_ATOMIC_ADD        2
#define ST_ATOMIC_SUB        3
#define ST_ATOMIC_INC        4
#define ST_ATOMIC_DEC        5

#ifdef _SEM_SEMUN_UNDEFINED
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};
#endif

void *alloc_buf(ssize_t size)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

void free_buf(void *addr, ssize_t size)
{
    munmap(addr, size);
}

void set_process_to_cpu(int cpu_id)
{
    int cpu_count = sysconf(_SC_NPROCESSORS_CONF);

    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(cpu_id % cpu_count, &set);

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

st_test(atomic, load) {

    int64_t *value = alloc_buf(sizeof(int64_t));

    for (int i = 0; i < 10000; i++) {
        *value = i;
        st_ut_eq(i, st_atomic_load(value), "");
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, store) {

    int64_t *value = alloc_buf(sizeof(int64_t));

    for (int i = 0; i < 10000; i++) {
        st_atomic_store(value, i);

        st_ut_eq(i, *value, "");
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, st_atomic_fetch_add) {

    int64_t *value = alloc_buf(sizeof(int64_t));
    int64_t prev = 0;
    int64_t ret;

    *value = prev;

    for (int i = 0; i < 10000; i++) {
        ret = st_atomic_fetch_add(value, i);

        st_ut_eq(prev, ret, "");
        st_ut_eq(ret + i, *value, "");

        prev += i;
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, st_atomic_add) {

    int64_t *value = alloc_buf(sizeof(int64_t));
    int64_t prev = 0;
    int64_t ret;

    *value = prev;

    for (int i = 0; i < 10000; i++) {
        ret = st_atomic_add(value, i);

        st_ut_eq(prev + i, ret, "");
        st_ut_eq(ret, *value, "");

        prev += i;
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, st_atomic_fetch_sub) {

    int64_t *value = alloc_buf(sizeof(int64_t));
    int64_t prev = 1000000000;
    int64_t ret;

    *value = prev;

    for (int i = 0; i < 10000; i++) {
        ret = st_atomic_fetch_sub(value, i);

        st_ut_eq(prev, ret, "");
        st_ut_eq(ret - i, *value, "");

        prev -= i;
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, st_atomic_sub) {

    int64_t *value = alloc_buf(sizeof(int64_t));
    int64_t prev = 1000000000;
    int64_t ret;

    *value = prev;

    for (int i = 0; i < 10000; i++) {
        ret = st_atomic_sub(value, i);

        st_ut_eq(prev - i, ret, "");
        st_ut_eq(ret, *value, "");

        prev -= i;
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, st_atomic_swap) {

    int64_t *value = alloc_buf(sizeof(int64_t));
    int64_t prev = 0;
    int64_t ret;

    *value = prev;

    for (int i = 0; i < 10000; i++) {
        ret = st_atomic_swap(value, i);

        st_ut_eq(prev, ret, "");
        st_ut_eq(i, *value, "");

        prev = *value;
    }

    free_buf(value, sizeof(int64_t));
}

st_test(atomic, st_atomic_cas) {

    int64_t *value = alloc_buf(sizeof(int64_t));
    int64_t *expect_v = alloc_buf(sizeof(int64_t));

    int64_t ret;

    *value = -2;
    *expect_v = -1;

    for (int i = 0; i < 10000; i++) {

        st_ut_ne(*expect_v, *value, "");

        ret = st_atomic_cas(value, expect_v, i);
        st_ut_eq(0, ret, "");
        st_ut_eq(*value, *expect_v, "");
        st_ut_ne(*expect_v, i, "");

        ret = st_atomic_cas(value, expect_v, i);
        st_ut_eq(1, ret, "");
        st_ut_eq(i, *value, "");
        st_ut_ne(*expect_v, *value, "");
    }

    free_buf(value, sizeof(int64_t));
    free_buf(expect_v, sizeof(int64_t));
}

int block_all_children(int sem_id)
{
    union semun sem_args;
    sem_args.val = 0;

    int ret = semctl(sem_id, 0, SETVAL, sem_args);
    if (ret == -1) {
        return ST_ERR;
    }

    return ST_OK;
}

int wakeup_all_children(int sem_id, int children_num)
{
    struct sembuf sem = {.sem_num = 0, .sem_op = children_num, .sem_flg = SEM_UNDO};

    int ret = semop(sem_id, &sem, 1);
    if (ret == -1) {
        return ST_ERR;
    }

    return ST_OK;
}

int wait_sem(int sem_id)
{
    struct sembuf sem = {.sem_num = 0, .sem_op = -1, .sem_flg = SEM_UNDO};

    int ret = semop(sem_id, &sem, 1);
    if (ret == -1) {
        return ST_ERR;
    }

    return ST_OK;
}

int test_add_sub_in_process(int64_t init_v, int64_t step_v, int op)
{
    int ret;
    int child;
    int pids[10] = {0};

    int sem_id = semget(5678, 1, 06666|IPC_CREAT);
    if (sem_id == -1) {
        return ST_ERR;
    }

    ret = block_all_children(sem_id);
    if (ret == ST_ERR) {
        return ret;
    }

    int64_t *value = alloc_buf(sizeof(int64_t));
    *value = init_v;

    for (int i = 0; i < 10; i++) {

        child = fork();

        if (child == 0) {
            set_process_to_cpu(i);

            ret = wait_sem(sem_id);
            if (ret == ST_ERR) {
                return ret;
            }

            for (int j = 0; j < 100000000; j++) {

                switch (op) {
                    case ST_ATOMIC_FETCH_ADD:
                        st_atomic_fetch_add(value, step_v);
                        break;

                    case ST_ATOMIC_FETCH_SUB:
                        st_atomic_fetch_sub(value, step_v);
                        break;

                    case ST_ATOMIC_ADD:
                        st_atomic_add(value, step_v);
                        break;

                    case ST_ATOMIC_SUB:
                        st_atomic_sub(value, step_v);
                        break;

                    case ST_ATOMIC_INC:
                        st_atomic_incr(value, step_v);
                        break;

                    case ST_ATOMIC_DEC:
                        st_atomic_decr(value, step_v);
                        break;
                }
            }

            exit(ST_OK);
        }

        pids[i] = child;
    }

    sleep(2);

    ret = wakeup_all_children(sem_id, 10);
    if (ret != ST_OK) {
        goto error;
    }

    ret = wait_children(pids, 10);
    if (ret != ST_OK) {
        goto error;
    }

    int64_t expect_v;

    if (op == ST_ATOMIC_FETCH_ADD || op == ST_ATOMIC_ADD || op == ST_ATOMIC_INC) {
        expect_v = init_v + step_v * 100000000 * 10;
    } else {
        expect_v = init_v - step_v * 100000000 * 10;
    }

    if (expect_v != *value) {
        ret = ST_ERR;
    }

error:
    semctl(sem_id, 0, IPC_RMID);
    free_buf(value, sizeof(int64_t));

    return ret;
}

st_test(atomic, add_sub_in_multi_process) {

    struct case_s {
        int64_t init_v;
        int64_t step_v;
        int op;
    } cases[] = {
        {0, 3, ST_ATOMIC_FETCH_ADD},
        {0, 3, ST_ATOMIC_ADD},
        {0, 1, ST_ATOMIC_INC},
        {0, 3, ST_ATOMIC_INC},

        {200000, 3, ST_ATOMIC_FETCH_SUB},
        {200000, 3, ST_ATOMIC_SUB},
        {200000, 1, ST_ATOMIC_DEC},
        {200000, 3, ST_ATOMIC_DEC},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(ST_OK, test_add_sub_in_process(c.init_v, c.step_v, c.op), "");
    }
}


st_test(atomic, store_load_in_multi_process) {

    int i, child, ret;
    int pids[10] = {0};

    int64_t store_values[5] = {0xef, 0xefaf, 0x2ffefaff, 0xaf1ffbff3f9f, 0xafffffffffffffff};
    int64_t *value = alloc_buf(sizeof(int64_t));
    *value = store_values[0];

    int sem_id = semget(6789, 1, 06666|IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    st_ut_eq(ST_OK, block_all_children(sem_id), "");

    for (i = 0; i < 10; i++) {

        child = fork();

        if (child == 0) {
            int found = 0;
            int64_t v;

            set_process_to_cpu(i);

            ret = wait_sem(sem_id);
            if (ret == ST_ERR) {
                exit(ret);
            }

            for (int j = 0; j < 100000000; j++) {

                if (j % 2 == 0) {
                    st_atomic_store(value, store_values[j % 5]);

                } else {

                    v = st_atomic_load(value);

                    for (int k = 0; k < 5; k++) {
                        if (v == store_values[k]) {
                            found = 1;
                            break;
                        }
                    }

                    if (found == 0) {
                        exit(ST_ERR);
                    }
                }
            }

            exit(ST_OK);
        }

        pids[i] = child;
    }

    sleep(2);

    st_ut_eq(ST_OK, wakeup_all_children(sem_id, 10), "");

    st_ut_eq(ST_OK, wait_children(pids, 10), "");

    semctl(sem_id, 0, IPC_RMID);
    free_buf(value, sizeof(int64_t));
}

st_test(atomic, swap_in_multi_process) {

    int child, found, ret;
    int pids[10] = {0};

    int64_t store_values[5] = {0xef, 0xefaf, 0x2ffefaff, 0xaf1ffbff3f9f, 0xafffffffffffffff};
    int64_t *value = alloc_buf(sizeof(int64_t));
    *value = 0;

    int sem_id = semget(7890, 1, 06666|IPC_CREAT);
    st_ut_ne(-1, sem_id, "");

    st_ut_eq(ST_OK, block_all_children(sem_id), "");

    for (int i = 0; i < 10; i++) {

        child = fork();

        if (child == 0) {
            set_process_to_cpu(i);

            ret = wait_sem(sem_id);
            if (ret == ST_ERR) {
                exit(ret);
            }

            for (int j = 0; j < 100000000; j++) {
                st_atomic_swap(value, store_values[j % 5]);

                st_atomic_cas(value, &store_values[j % 5], store_values[(j + 1) % 5]);
            }

            exit(ST_OK);
        }

        pids[i] = child;
    }

    sleep(2);

    st_ut_eq(ST_OK, wakeup_all_children(sem_id, 10), "");

    st_ut_eq(ST_OK, wait_children(pids, 10), "");

    found = 0;

    for (int i = 0; i < 5; i++) {
        if (*value == store_values[i]) {
            found = 1;
        }
    }

    st_ut_eq(1, found, "");

    semctl(sem_id, 0, IPC_RMID);
    free_buf(value, sizeof(int64_t));
}

st_ut_main;
