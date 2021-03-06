#include "array.h"
#include "unittest/unittest.h"

#define test_callback_inited {.pool = NULL,\
                              .realloc = _realloc,\
                              .free = _free,}

int compare(const void *a, const void *b) {
    int m = *(int *) a;
    int n = *(int *) b;

    return m - n;
}

int compare_reverse(const void *a, const void *b) {
    int m = *(int *) a;
    int n = *(int *) b;

    return n - m;
}

st_test(array, static_array_init) {

    st_array_t array = {0};
    int array_buf[10];

    struct case_s {
        st_array_t *array;
        int total_count;
        void *start_addr;
        int expect_ret;
    } cases[] = {
        {NULL, 10, array_buf, ST_ARG_INVALID},
        {&array, 10, NULL, ST_ARG_INVALID},
        {&array, 0, array_buf, ST_ARG_INVALID},
        {&array, 10, array_buf, ST_OK},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        if (c.expect_ret != ST_OK) {
            st_ut_bug(st_array_init_static(c.array, 4, c.start_addr, c.total_count,
                                           compare),
                      "test static init with invalid arg");
            continue;
        }
        else {
            st_array_init_static(c.array, 4, c.start_addr, c.total_count,
                                 compare);
        }

        st_ut_eq(c.start_addr, array.start_addr, "start_addr ok");
        st_ut_eq(4, array.element_size, "element_size ok");
        st_ut_eq(0, array.dynamic, "dynamic ok");
        st_ut_eq(c.total_count, array.total_cnt, "total_count ok");
        st_ut_eq(0, array.current_cnt, "total_count ok");
        st_ut_eq(1, array.inited, "inited ok");
    }
}

void *_realloc(void *pool, void *ptr, size_t size) {
    return realloc(ptr, size);
}

void _free(void *pool, void *ptr) {
    free(ptr);
}

st_test(array, dynamic_array_init) {

    st_array_t array = {0};
    st_callback_memory_t uninited_callback = {0};
    st_callback_memory_t callback = test_callback_inited;

    struct case_s {
        st_array_t *array;
        st_callback_memory_t callback;
        int expect_ret;
    } cases[] = {
        {NULL, callback, ST_ARG_INVALID},
        {&array, uninited_callback, ST_ARG_INVALID},
        {&array, callback, ST_OK},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.expect_ret,
                 st_array_init_dynamic(c.array, 4, c.callback, compare),
                 "test dynamic init");

        if (c.expect_ret != ST_OK) {
            continue;
        }

        st_ut_ne(NULL, array.start_addr, "start_addr ok");
        st_ut_eq(4, array.element_size, "element_size ok");
        st_ut_eq(1, array.dynamic, "dynamic ok");
        st_ut_eq(64, array.total_cnt, "total_count ok");
        st_ut_eq(0, array.current_cnt, "total_count ok");
        st_ut_eq(0, memcmp(&c.callback, &array.callback, sizeof(c.callback)), "callback_func ok");
        st_ut_eq(1, array.inited, "inited ok");
    }
}

st_test(array, destroy) {

    st_array_t array = {0};
    st_callback_memory_t callback = test_callback_inited;
    int array_buf[10];
    int tmp  = 12;

    struct case_s {
        st_array_t *array;
        int dynamic;
        int append;
        int expect_ret;
    } cases[] = {
        {NULL, 0, 0, ST_ARG_INVALID},

        {&array, 0, 0, ST_OK},
        {&array, 0, 1, ST_OK},

        {&array, 1, 0, ST_OK},
        {&array, 1, 1, ST_OK},
    };

    st_ut_nobug(st_array_destroy(&array), "destroy uninited");

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        if (c.dynamic == 1) {
            st_ut_eq(ST_OK, st_array_init_dynamic(&array, 4, callback, compare), "init ok");
        } else {
            st_array_init_static(&array, 4, array_buf, 10, compare);
        }

        if (c.append == 1) {
            st_ut_eq(ST_OK, st_array_append(&array, &tmp), "append value");
        }

        if (c.array != NULL) {
            st_ut_nobug(st_array_destroy(c.array), "destroy array ok");
        }

        if (c.expect_ret != ST_OK) {
            continue;
        }

        st_ut_eq(0, array.element_size, "element_size ok");
        st_ut_eq(0, array.dynamic, "dynamic ok");
        st_ut_eq(NULL, array.start_addr, "start_addr ok");
        st_ut_eq(0, array.total_cnt, "total_count ok");
        st_ut_eq(0, array.current_cnt, "total_count ok");
        st_ut_eq(0, array.inited, "inited ok");
    }
}

st_test(array, dynamic_array_expand) {
    st_array_t array = {0};
    int append_buf[66] = {0};
    void *prev_start = NULL;
    st_callback_memory_t callback = test_callback_inited;

    struct case_s {
        int append_num;
        int expect_curr_cnt;
        int expect_total_cnt;
        int array_start_changed;
    } cases[] = {
        {1, 1, 64, 1},
        {63, 64, 64, 0},
        {66, 130, 130, -1},
        {1, 131, 194, -1},
    };

    st_array_init_dynamic(&array, 4, callback, compare);

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(ST_OK, st_array_append(&array, append_buf, c.append_num), "append values");

        st_ut_eq(c.expect_curr_cnt, array.current_cnt, "array current_cnt is right");
        st_ut_eq(c.expect_total_cnt, array.total_cnt, "array total_cnt is right");

        // realloc memory address maybe same as old memory
        if (c.array_start_changed != -1) {
            st_ut_eq(c.array_start_changed, array.start_addr != prev_start, "array start is right");
        }

        prev_start = array.start_addr;
    }
}

st_test(array, static_array_append) {
    st_array_t array = {0};
    int ret;
    int array_buf[10] = {0};
    int append_buf[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    struct case_s {
        int append_num;
        int expect_curr_cnt;
        int expect_total_cnt;
        int expect_status;
    } cases[] = {
        {1, 1, 10, ST_OK},
        {2, 3, 10, ST_OK},
        {6, 9, 10, ST_OK},
        {2, 9, 10, ST_OUT_OF_MEMORY},
        {1, 10, 10, ST_OK},
    };

    st_ut_eq(ST_UNINITED, st_array_append(&array, append_buf), "append uninited array");

    st_array_init_static(&array, 4, array_buf, 10, compare);

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        if (c.append_num == 1) {
            ret = st_array_append(&array, &append_buf[array.current_cnt]);
        } else {
            ret = st_array_append(&array, &append_buf[array.current_cnt], c.append_num);
        }

        st_ut_eq(c.expect_curr_cnt, array.current_cnt, "array current_cnt is right");
        st_ut_eq(c.expect_total_cnt, array.total_cnt, "array total_cnt is right");
        st_ut_eq(c.expect_status, ret, "array append status is right");

        if (c.expect_status != ST_OK) {
            continue;
        }

        for (int j = 0; j < array.current_cnt; j++) {
            st_ut_eq(j, *(int *)st_array_get(&array, j), "array elements is right");
        }
    }
}

// just test append in scalable mode, other case has been tested in static_array_append
st_test(array, dynamic_array_append) {
    st_array_t array = {0};
    int append_buf[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    st_callback_memory_t callback = test_callback_inited;

    st_array_init_dynamic(&array, 4, callback, compare);

    st_ut_eq(ST_OK, st_array_append(&array, &append_buf, 10), "append values");
    st_ut_eq(10, array.current_cnt, "current_cnt right");

    for (int i = 0; i < array.current_cnt; i++) {
        st_ut_eq(i, *(int *)st_array_get(&array, i), "array elements is right");
    }
}

st_test(array, static_array_insert) {
    st_array_t array = {0};
    int ret;
    int array_buf[20] = {0};
    int append_buf[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int insert_buf[11] = {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};

    struct case_s {
        int insert_idx;
        int insert_num;
        int expect_curr_cnt;
        int expect_total_cnt;
        int expect_status;
    } cases[] = {
        {0, 1, 11, 20, ST_OK},
        {5, 1, 11, 20, ST_OK},
        {10, 1, 11, 20, ST_OK},

        {0, 10, 20, 20, ST_OK},
        {5, 10, 20, 20, ST_OK},
        {10, 10, 20, 20, ST_OK},

        {10, 11, 10, 20, ST_OUT_OF_MEMORY},
        {11, 10, 10, 20, ST_INDEX_OUT_OF_RANGE},
    };

    st_ut_eq(ST_UNINITED, st_array_insert(&array, 0, insert_buf), "insert uninited array");

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_array_init_static(&array, 4, array_buf, 20, compare);
        st_ut_eq(ST_OK, st_array_append(&array, append_buf, 10), "append ok");

        if (c.insert_num == 1) {
            ret = st_array_insert(&array, c.insert_idx, insert_buf);
        } else {
            ret = st_array_insert(&array, c.insert_idx, insert_buf, c.insert_num);
        }

        st_ut_eq(c.expect_curr_cnt, array.current_cnt, "array current_cnt is right");
        st_ut_eq(c.expect_total_cnt, array.total_cnt, "array total_cnt is right");
        st_ut_eq(c.expect_status, ret, "array insert status is right");

        if (c.expect_status != ST_OK) {
            st_array_destroy(&array);
            continue;
        }

        for (int j = 0; j < array.current_cnt; j++) {
            if (j < c.insert_idx) {
                st_ut_eq(j, *(int *)st_array_get(&array, j), "array elements is right");
            } else if (j < c.insert_idx + c.insert_num) {
                // insert value is begin with 20 plus
                st_ut_eq(j - c.insert_idx + 20, *(int *)st_array_get(&array, j), "array elements is right");
            } else {
                st_ut_eq(j - c.insert_num, *(int *)st_array_get(&array, j), "array elements is right");
            }
        }

        st_array_destroy(&array);
    }
}

// just test insert in scalable mode, other case has been tested in static_array_insert
st_test(array, dynamic_array_insert) {
    st_array_t array = {0};
    st_callback_memory_t callback = test_callback_inited;
    int append_buf[10] = {0, 1, 2, 3, 4, 10, 11, 12, 13, 14};
    int insert_buf[5] = {5, 6, 7, 8, 9};

    st_array_init_dynamic(&array, 4, callback, compare);

    st_ut_eq(ST_OK, st_array_append(&array, append_buf, 10), "append values");

    st_ut_eq(ST_OK, st_array_insert(&array, 5, insert_buf, 5), "insert values");
    st_ut_eq(15, array.current_cnt, "current_cnt right");

    for (int i = 0; i < array.current_cnt; i++) {
        st_ut_eq(i, *(int *)st_array_get(&array, i), "array elements is right");
    }
}

st_test(array, remove) {
    st_array_t array = {0};
    int array_buf[20] = {0};
    int append_buf[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    struct case_s {
        int remove_idx;
        int remove_num;
        int expect_curr_cnt;
        int expect_total_cnt;
    } cases[] = {
        {0,  1,  9,  20},
        {5,  1,  9,  20},
        {9,  1,  9,  20},
        {0,  5,  5,  20},
        {5,  5,  5,  20},
        {8,  2,  8,  20},
        {0,  10, 0,  20},
        {10, 1,  10, 20},
        {9,  10, 9,  20},
    };

    st_ut_bug(st_array_remove(&array, 0), "remove uninited array");

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_array_init_static(&array, 4, array_buf, 20, compare);
        st_ut_eq(ST_OK, st_array_append(&array, append_buf, 10), "append ok");
        st_ut_bug(st_array_remove(&array, -1), "remove out of index");
        if (c.remove_idx < 0 || c.remove_idx >= array.current_cnt) {
            st_ut_bug(st_array_remove(&array, c.remove_idx));
        }
        else {
            if (c.remove_num == 1) {
                st_array_remove(&array, c.remove_idx);
            } else {
                st_array_remove(&array, c.remove_idx, c.remove_num);
            }

            st_ut_eq(c.expect_curr_cnt, array.current_cnt, "array current_cnt is right");
            st_ut_eq(c.expect_total_cnt, array.total_cnt, "array total_cnt is right");
        }

        for (int j = 0; j < array.current_cnt; j++) {
            if (j < c.remove_idx) {
                st_ut_eq(j, *(int *)st_array_get(&array, j), "array elements is right");
            } else {
                st_ut_eq(j + c.remove_num, *(int *)st_array_get(&array, j), "array elements is right");
            }
        }

        st_array_destroy(&array);
    }
}

st_test(array, sort) {
    st_array_t array = {0};
    int array_buf[20] = {0};
    int append_buf[10] = {8, 7, 5, 3, 4, 1, 2, 6, 9, 0};

    st_ut_bug(st_array_sort(&array, NULL), "compare uninited array");

    st_array_init_static(&array, 4, array_buf, 20, compare);
    st_ut_eq(ST_OK, st_array_append(&array, append_buf, 10), "append ok");

    st_ut_nobug(st_array_sort(&array, NULL), "sort ok");
    st_ut_eq(10, array.current_cnt, "current_cnt right");

    for (int i = 0; i < array.current_cnt; i++) {
        st_ut_eq(i, *(int *)st_array_get(&array, i), "array elements is right");
    }

    st_ut_nobug(st_array_sort(&array, compare_reverse), "sort ok");

    for (int i = 0; i < array.current_cnt; i++) {
        st_ut_eq(array.current_cnt - i - 1, *(int *)st_array_get(&array, i), "array elements is right");
    }

    st_ut_bug(st_array_sort(NULL, NULL), "sort array NULL");
}

st_test(array, bsearch_left_right) {
    st_array_t array = {0};
    int array_buf[20] = {0};
    int append_buf[10] = {2, 5, 10, 10, 10, 10, 14, 19, 23, 43};
    int tmp = 0;

    struct case_s {
        int value;
        int left;
        int right;
        int found;
    } cases[] = {
        {1,  0,  0,  ST_NOT_FOUND },
        {2,  0,  1,  ST_OK        },
        {5,  1,  2,  ST_OK        },
        {10, 2,  6,  ST_OK        },
        {13, 6,  6,  ST_NOT_FOUND },
        {14, 6,  7,  ST_OK        },
        {19, 7,  8,  ST_OK        },
        {43, 9,  10, ST_OK        },
        {45, 10, 10, ST_NOT_FOUND },
    };

    ssize_t idx;

    st_ut_eq(ST_UNINITED, st_array_bsearch_left(&array, &tmp, NULL, &idx), "bsearch uninited array");
    st_ut_eq(ST_UNINITED, st_array_bsearch_right(&array, &tmp, NULL, &idx), "bsearch uninited array");

    st_array_init_static(&array, 4, array_buf, 20, compare);
    st_ut_eq(ST_OK, st_array_append(&array, append_buf, 10), "append ok");

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.found, st_array_bsearch_left(&array, &c.value, NULL, &idx), "");
        st_ut_eq(c.left, idx, "bsearch left");

        st_ut_eq(c.found, st_array_bsearch_right(&array, &c.value, NULL, &idx), "");
        st_ut_eq(c.right, idx, "bsearch right");

    }

    st_ut_eq(ST_ARG_INVALID, st_array_bsearch_left(NULL, &tmp, NULL, &idx), "bsearch array NULL");
    st_ut_eq(ST_ARG_INVALID, st_array_bsearch_left(&array, NULL, NULL, &idx), "bsearch element NULL");

    st_ut_eq(ST_ARG_INVALID, st_array_bsearch_right(NULL, &tmp, NULL, &idx), "bsearch array NULL");
    st_ut_eq(ST_ARG_INVALID, st_array_bsearch_right(&array, NULL, NULL, &idx), "bsearch element NULL");
}

st_test(array, indexof) {
    st_array_t array = {0};
    int array_buf[20] = {0};
    int append_buf[10] = {8, 7, 5, 3, 4, 1, 2, 6, 9, 0};
    int tmp;

    struct case_s {
        int value;
        int idx;
    } cases[] = {
        {8, 0},
        {7, 1},
        {5, 2},
        {3, 3},
        {4, 4},
        {1, 5},
        {2, 6},
        {6, 7},
        {9, 8},
        {0, 9},
    };

    ssize_t idx;

    st_ut_eq(ST_UNINITED, st_array_indexof(&array, &tmp, NULL, &idx), "indexof uninited array");

    st_array_init_static(&array, 4, array_buf, 20, compare);
    st_ut_eq(ST_OK, st_array_append(&array, append_buf, 10), "append ok");

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        tmp = c.value;

        st_ut_eq(ST_OK, st_array_indexof(&array, &tmp, NULL, &idx), "");
        st_ut_eq(c.idx, idx, "search ok");
    }

    tmp = 10;
    st_ut_eq(ST_NOT_FOUND, st_array_indexof(&array, &tmp, NULL, &idx), "search ok");

    st_ut_eq(ST_ARG_INVALID, st_array_indexof(NULL, &tmp, NULL, &idx), "bsearch array NULL");
    st_ut_eq(ST_ARG_INVALID, st_array_indexof(&array, NULL, NULL, &idx), "bsearch element NULL");
}

st_ut_main;
