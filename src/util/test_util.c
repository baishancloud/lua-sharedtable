#include "inc/inc.h"
#include "unittest/unittest.h"

st_test(util, st_nelts) {

    int n;

    { int arr[10];       n = st_nelts(arr); ddx(n); st_ut_eq(10, n, ""); }
    { int *arr[10];      n = st_nelts(arr); ddx(n); st_ut_eq(10, n, ""); }
    { int arr[10][10];   n = st_nelts(arr); ddx(n); st_ut_eq(10, n, ""); }
    { void *arr[10][10]; n = st_nelts(arr); ddx(n); st_ut_eq(10, n, ""); }
}

st_test(util, st_unused) {
    int n;
    /* no warning */
    st_unused(n);
}

st_test(util, st_align) {

    struct case_s {
        uint64_t inp;
        uint64_t upto;
        uint64_t expected;
    } cases[] = {
        {0, 0, 0},
        {0, 1, 0},
        {0, 2, 0},
        {1, 2, 2},
        {2, 2, 2},
        {0, 4, 0},
        {1, 4, 4},
        {2, 4, 4},
        {3, 4, 4},
        {4, 4, 4},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0])   c = cases[i];
        st_typeof(c.expected) rst = st_align(c.inp, c.upto);

        ddx(rst);

        st_ut_eq(c.expected, rst, "");
    }
}

st_test(util, cmp) {

    struct case_s {
        int64_t a;
        int64_t b;
        int64_t expected;
    } cases[] = {
        {0, 0,   0},
        {1, 0,   1},
        {0, 1,   -1},
        {1, 1,   0},
        {-1, -1, 0},
        {-1, 0,  -1},
        {0, -1,  1},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0])   c = cases[i];
        st_typeof(c.expected) rst = st_cmp(c.a, c.b);

        ddx(rst);

        st_ut_eq(c.expected, rst, "");
    }
}

/*
 * TODO
 *     offset
 *     by_offset
 *     owner
 *     min
 *     max
 *     foreach
 */

st_ut_main;
