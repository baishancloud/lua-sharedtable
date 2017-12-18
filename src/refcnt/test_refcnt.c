#include <stdlib.h>
#include "refcnt.h"
#include "unittest/unittest.h"

#define test_callback_inited {.pool = NULL,\
                              .alloc_with_ret = _alloc,\
                              .free_with_ret = _free,}

int _alloc(void *pool, size_t size, void **ptr) {
    *ptr = malloc(size);
    return ST_OK;
}

int _free(void *pool, void *ptr) {
    free(ptr);
    return ST_OK;
}

st_test(refcnt, init) {

    st_refcnt_t refcnt;
    st_callback_memory_t callback = test_callback_inited;

    st_ut_eq(ST_OK, st_refcnt_init(&refcnt, callback), "");
    st_ut_eq(0, memcmp(&refcnt.callback, &callback, sizeof(callback)), "");
    st_ut_eq(refcnt.total_cnt, 0, "");

    st_ut_eq(ST_ARG_INVALID, st_refcnt_init(NULL, callback), "");
    st_ut_eq(ST_ARG_INVALID, st_refcnt_init(&refcnt, (st_callback_memory_t) {.alloc_with_ret = NULL}),
    "");
    st_ut_eq(ST_ARG_INVALID, st_refcnt_init(&refcnt, (st_callback_memory_t) {.alloc_with_ret = _alloc}),
    "");
}

st_test(refcnt, destroy) {
    st_refcnt_t refcnt;
    st_callback_memory_t callback = test_callback_inited;
    st_refcnt_init(&refcnt, callback);

    st_refcnt_incr(&refcnt, getpid(), 2, NULL, NULL);
    st_refcnt_incr(&refcnt, getpid() + 1, 3, NULL, NULL);

    st_ut_eq(0, st_rbtree_is_empty(&refcnt.processes), "");
    st_ut_eq(5, refcnt.total_cnt, "");

    st_ut_eq(ST_OK, st_refcnt_destroy(&refcnt), "");

    st_ut_eq(1, st_rbtree_is_empty(&refcnt.processes), "");
    st_ut_eq(0, refcnt.total_cnt, "");
}

st_test(refcnt, incr_with_get) {

    int ret;
    st_refcnt_t refcnt;
    int64_t process_refcnt;
    int64_t total_refcnt;

    st_callback_memory_t callback = test_callback_inited;
    st_refcnt_init(&refcnt, callback);

    struct case_s {
        int64_t cnt;
        int64_t process_refcnt;
        int64_t total_refcnt;
        pid_t pid;
    } cases[] = {
        {1, 1, 1, 1234},
        {2, 2, 3, 2345},
        {2, 3, 5, 1234},
        {5, 5, 10, 3456},
        {5, 7, 15, 2345},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        ret = st_refcnt_incr(&refcnt, c.pid, c.cnt, &process_refcnt, &total_refcnt);
        st_ut_eq(ST_OK, ret, "");
        st_ut_eq(c.process_refcnt, process_refcnt, "");
        st_ut_eq(c.total_refcnt, total_refcnt, "");

        ret = st_refcnt_get_process_refcnt(&refcnt, c.pid, &process_refcnt);
        st_ut_eq(ST_OK, ret, "");
        st_ut_eq(c.process_refcnt, process_refcnt, "");

        ret = st_refcnt_get_total_refcnt(&refcnt, &total_refcnt);
        st_ut_eq(ST_OK, ret, "");
        st_ut_eq(c.total_refcnt, total_refcnt, "");
    }

    st_ut_eq(ST_ARG_INVALID, st_refcnt_incr(NULL, getpid(), 1, &process_refcnt, &total_refcnt), "");

    st_refcnt_destroy(&refcnt);
}

st_test(refcnt, decr_with_get) {

    int ret;
    st_refcnt_t refcnt;
    int64_t process_refcnt;
    int64_t total_refcnt;

    st_callback_memory_t callback = test_callback_inited;
    st_refcnt_init(&refcnt, callback);

    st_refcnt_incr(&refcnt, 1234, 10, NULL, NULL);
    st_refcnt_incr(&refcnt, 2345, 10, NULL, NULL);
    st_refcnt_incr(&refcnt, 3456, 10, NULL, NULL);

    struct case_s {
        int64_t cnt;
        int64_t process_refcnt;
        int64_t total_refcnt;
        pid_t pid;
        int expect_ret;
    } cases[] = {
        {1, 9, 29, 1234, ST_OK},
        {2, 8, 27, 2345, ST_OK},
        {2, 7, 25, 1234, ST_OK},
        {5, 5, 20, 3456, ST_OK},
        {5, 3, 15, 2345, ST_OK},
        {7, 0, 8, 1234, ST_OK},
        {6, 3, 8, 2345, ST_OUT_OF_RANGE},
        {3, 0, 5, 2345, ST_OK},
        {5, 0, 0, 3456, ST_OK},
        {5, 0, 0, 4567, ST_NOT_FOUND},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        ret = st_refcnt_decr(&refcnt, c.pid, c.cnt, &process_refcnt, &total_refcnt);
        st_ut_eq(c.expect_ret, ret, "");

        if (c.expect_ret != ST_OK) {
            continue;
        }

        st_ut_eq(c.process_refcnt, process_refcnt, "");
        st_ut_eq(c.total_refcnt, total_refcnt, "");

        if (process_refcnt == 0) {
            st_ut_eq(ST_NOT_FOUND, st_refcnt_get_process_refcnt(&refcnt, c.pid, &process_refcnt), "");
        } else {
            st_ut_eq(ST_OK, st_refcnt_get_process_refcnt(&refcnt, c.pid, &process_refcnt), "");
            st_ut_eq(c.process_refcnt, process_refcnt, "");
        }

        st_ut_eq(ST_OK, st_refcnt_get_total_refcnt(&refcnt, &total_refcnt), "");
        st_ut_eq(c.total_refcnt, total_refcnt, "");
    }

    st_ut_eq(1, st_rbtree_is_empty(&refcnt.processes), "");
    st_ut_eq(0, refcnt.total_cnt, "");
}

st_ut_main;
