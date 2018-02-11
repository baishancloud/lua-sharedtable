#include "str.h"
#include "unittest/unittest.h"

#define FOO "abc"

st_test(str, const) {

    {
        st_str_t s = st_str_const(FOO);

        st_ut_eq(0,             s.type,        "type is 0");
        st_ut_eq(sizeof(FOO)-1, s.len,         "len");
        st_ut_eq(sizeof(FOO),   s.capacity,    "capacity");
        st_ut_eq(0,             s.bytes_owned, "bytes_owned is 0");
        st_ut_ne(NULL,          s.bytes,       "bytes is not NULL");
    }

    {
        st_str_t s = st_str_var(10);

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(10,   s.len,         "len");
        st_ut_eq(10,   s.capacity,    "capacity");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_ne(NULL, s.bytes,       "bytes is not NULL");
    }

    {
        char     *b = FOO;
        int64_t   l = 2;
        st_str_t  s = st_str_wrap(b, l);

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(l,    s.len,         "len");
        st_ut_eq(l,    s.capacity,    "capacity");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_eq(b,    s.bytes,       "bytes is not NULL");
    }

    {
        char     *b = FOO;
        int64_t   l = sizeof(FOO) - 1;
        st_str_t  s = st_str_wrap_0(b, l);

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(l,    s.len,         "len");
        st_ut_eq(l+1,  s.capacity,    "capacity");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_eq(b,    s.bytes,       "bytes is b");

        st_ut_eq(1, st_str_trailing_0(&s), "it has trailing 0");
    }

    {
        char b[10];
        st_str_t s = st_str_wrap_chars(b);

        st_ut_eq(0,           s.type,        "type is 0");
        st_ut_eq(10,          s.len,         "len");
        st_ut_eq(10,          s.capacity,    "capacity");
        st_ut_eq(0,           s.bytes_owned, "bytes_owned is 0");
        st_ut_eq((uint8_t*)b, s.bytes,       "bytes is b");
    }

    {
        st_str_t s = st_str_null;

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(0,    s.len,         "len");
        st_ut_eq(0,    s.capacity,    "capacity");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_eq(NULL, s.bytes,       "bytes is NULL");
    }

    {
        st_str_t s = st_str_empty;

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(0,    s.len,         "len");
        st_ut_eq(1,    s.capacity,    "capacity");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_ne(NULL, s.bytes,       "bytes is not NULL");
    }

}

st_test(str, init_invalid) {

    {
        int ret = st_str_init(NULL, 1);
        st_ut_eq(ST_ARG_INVALID, ret, "s is NULL");
    }

    struct case_s {
        int64_t inp;
        int expected;
    } cases[] = {
        {-1, ST_ARG_INVALID},
        {-2, ST_ARG_INVALID},
        {-1024, ST_ARG_INVALID},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0])   c = cases[i];
        st_typeof(c.expected) rst;

        st_str_t s = {{0}};

        rst = st_str_init(&s, c.inp);
        ddx(rst);

        st_ut_eq(c.expected, rst, "");

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(NULL, s.right,       "right is NULL");
        st_ut_eq(0,    s.len,         "len is 0");
        st_ut_eq(0,    s.capacity,    "capacity is 0");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_eq(NULL, s.bytes,       "bytes is NULL");
    }
}

st_test(str, init) {

    int ret;

    {
        st_str_t s = st_str_null;
        ret = st_str_init(&s, 0);
        st_ut_eq(ST_OK, ret, "init ok");

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(0,    s.len,         "len is 0");
        st_ut_eq(1,    s.capacity,    "capacity is 1, because it use a pre-allocated empty c-string");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0, use pre-allocated empty c-string");
        st_ut_ne(NULL, s.bytes,       "bytes is not NULL");

        ret = st_str_destroy(&s);
        st_ut_eq(ST_OK, ret, "destroy");

        st_ut_eq(-1,   s.type,        "type is -1");
        st_ut_eq(0,    s.len,         "len is 0");
        st_ut_eq(0,    s.capacity,    "capacity is 0");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_eq(NULL, s.bytes,       "bytes is NULL");
    }

    {
        st_str_t s = st_str_null;
        ret = st_str_init(&s, 1);
        st_ut_eq(ST_OK, ret, "init ok");

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(1,    s.len,         "len is 0");
        st_ut_eq(1,    s.capacity,    "capacity is 1");
        st_ut_eq(1,    s.bytes_owned, "bytes_owned is 1");
        st_ut_ne(NULL, s.bytes,       "bytes is not NULL");

        ret = st_str_destroy(&s);
        st_ut_eq(ST_OK, ret, "destroy");
    }

    {
        st_str_t s = st_str_null;
        ret = st_str_init(&s, 102400);
        st_ut_eq(ST_OK, ret, "init ok");

        st_ut_eq(0,      s.type,        "type is 0");
        st_ut_eq(102400, s.len,         "len is 0");
        st_ut_eq(102400, s.capacity,    "capacity is 1, because it use a pre-allocated empty c-string");
        st_ut_eq(1,      s.bytes_owned, "bytes_owned is 1");
        st_ut_ne(NULL,   s.bytes,       "bytes is not NULL");

        ret = st_str_destroy(&s);
        st_ut_eq(ST_OK, ret, "destroy");
    }
}

st_test(str, init_0) {

    int ret;

    {
        st_str_t s = st_str_null;
        ret = st_str_init_0(&s, 0);
        st_ut_eq(ST_OK, ret, "init ok");

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(0,    s.len,         "len is 0");
        st_ut_eq(1,    s.capacity,    "capacity is 1, because it use a pre-allocated empty c-string");
        st_ut_eq(1,    s.bytes_owned, "bytes_owned is 0, use pre-allocated empty c-string");
        st_ut_ne(NULL, s.bytes,       "bytes is not NULL");

        ret = st_str_destroy(&s);
        st_ut_eq(ST_OK, ret, "destroy");
    }

    {
        st_str_t s = st_str_null;
        ret = st_str_init_0(&s, 1);
        st_ut_eq(ST_OK, ret, "init ok");

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(1,    s.len,         "len");
        st_ut_eq(2,    s.capacity,    "capacity is 2");
        st_ut_eq(1,    s.bytes_owned, "bytes_owned is 1");
        st_ut_ne(NULL, s.bytes,       "bytes is not NULL");

        ret = st_str_destroy(&s);
        st_ut_eq(ST_OK, ret, "destroy");
    }

    {
        st_str_t s = st_str_null;
        ret = st_str_init_0(&s, 102400);
        st_ut_eq(ST_OK, ret, "init ok");

        st_ut_eq(0,      s.type,        "type is 0");
        st_ut_eq(102400, s.len,         "len");
        st_ut_eq(102401, s.capacity,    "capacity");
        st_ut_eq(1,      s.bytes_owned, "bytes_owned is 1");
        st_ut_ne(NULL,   s.bytes,       "bytes is not NULL");

        ret = st_str_destroy(&s);
        st_ut_eq(ST_OK, ret, "destroy");
    }
}

st_test(str, init_0_invalid) {

    {
        int ret = st_str_init_0(NULL, 1);
        st_ut_eq(ST_ARG_INVALID, ret, "s is NULL");
    }


    struct case_s {
        int64_t inp;
        int expected;
    } cases[] = {
        {-1, ST_ARG_INVALID},
        {-2, ST_ARG_INVALID},
        {-1024, ST_ARG_INVALID},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0])   c = cases[i];
        st_typeof(c.expected) rst;

        st_str_t s = {{0}};

        rst = st_str_init_0(&s, c.inp);

        st_ut_eq(c.expected, rst, "");

        st_ut_eq(0,    s.type,        "type is 0");
        st_ut_eq(NULL, s.right,       "right is NULL");
        st_ut_eq(0,    s.len,         "len is 0");
        st_ut_eq(0,    s.capacity,    "capacity is 0");
        st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
        st_ut_eq(NULL, s.bytes,       "bytes is NULL");
    }
}

st_test(str, equal) {

    {
        st_str_t *emp = &(st_str_t)st_str_empty;
        st_str_t *nu = &(st_str_t)st_str_null;

        st_ut_eq(0, st_str_equal(NULL, emp), "");
        st_ut_eq(0, st_str_equal(emp, NULL), "");
        st_ut_eq(0, st_str_equal(NULL, nu), "");
        st_ut_eq(0, st_str_equal(nu, NULL), "");
        st_ut_eq(0, st_str_equal(emp, nu), "");
        st_ut_eq(0, st_str_equal(nu, emp), "");

    }

    struct case_s {
        char *a;
        char *b;
        int expected;
    } cases[] = {
        {"",   "",   1},
        {"",   "a",  0},
        {"a",  "",   0},
        {"a",  "a",  1},
        {"aa", "a",  0},
        {"a",  "aa", 0},
        {"aa", "aa", 1},
        {"ab", "aa", 0},
        {"aa", "ab", 0},
        {"ab", "ab", 1},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0])   c = cases[i];
        st_typeof(c.expected) rst;

        st_str_t a = st_str_wrap(c.a, strlen(c.a));
        st_str_t b = st_str_wrap(c.b, strlen(c.b));
        rst = st_str_equal(&a, &b);

        ddx(rst);

        st_ut_eq(c.expected, rst, "");
    }
}

st_test(str, cmp) {

    {
        st_str_t *emp = &(st_str_t)st_str_empty;
        st_str_t *nu  = &(st_str_t)st_str_null;

        st_ut_eq(ST_ARG_INVALID, st_str_cmp(NULL, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_cmp(NULL, emp),  "");
        st_ut_eq(ST_ARG_INVALID, st_str_cmp(emp,  NULL), "");

        st_ut_eq(1,  st_str_cmp(emp, nu),  "");
        st_ut_eq(0,  st_str_cmp(emp, emp), "");
        st_ut_eq(-1, st_str_cmp(nu,  emp), "");
    }

    {
        st_str_t *emp1 = &(st_str_t)st_str_empty;
        st_str_t *emp2 = &(st_str_t)st_str_empty;

        emp1->type = 0;
        emp2->type = 0;

        st_ut_eq(0, st_str_cmp(emp1, emp2), "");

        emp1->type = 1;
        emp2->type = 0;
        st_ut_eq(1, st_str_cmp(emp1, emp2), "");

        emp1->type = 0;
        emp2->type = 1;
        st_ut_eq(-1, st_str_cmp(emp1, emp2), "");
    }

    struct case_s {
        char *a;
        char *b;
        int expected;
    } cases[] = {
        {"",   "",   0},
        {"",   "a",  -1},
        {"a",  "",   1},
        {"a",  "a",  0},
        {"aa", "a",  1},
        {"a",  "aa", -1},
        {"aa", "aa", 0},
        {"ab", "aa", 1},
        {"aa", "ab", -1},
        {"ab", "ab", 0},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0])   c = cases[i];
        st_typeof(c.expected) rst;

        st_str_t a = st_str_wrap(c.a, strlen(c.a));
        st_str_t b = st_str_wrap(c.b, strlen(c.b));
        rst = st_str_cmp(&a, &b);

        ddx(rst);

        st_ut_eq(c.expected, rst, "");
    }
}

st_test(str, trailing_0) {

    st_str_t s = st_str_const(FOO);
    st_ut_eq(1, st_str_trailing_0(&s), "");

    s.len += 1;
    st_ut_eq(0, st_str_trailing_0(&s), "");

    /* len = capacity-2, invalid str */
    s.len -= 2;
    st_ut_eq(0, st_str_trailing_0(&s), "");
}

st_test(str, destroy) {

    {
        st_ut_eq(ST_ARG_INVALID, st_str_destroy(NULL), "destroy NULL");
    }

    int ret;

    st_str_t s = st_str_null;
    ret = st_str_init(&s, 10);
    st_ut_eq(ST_OK, ret, "init ok");

    st_ut_eq(0,    s.type,        "type is 0");
    st_ut_eq(10,   s.len,         "len is 10");
    st_ut_eq(10,   s.capacity,    "capacity is 10");
    st_ut_eq(1,    s.bytes_owned, "bytes_owned is 1");
    st_ut_ne(NULL, s.bytes,       "bytes is not NULL");

    ret = st_str_destroy(&s);
    st_ut_eq(ST_OK, ret, "destroy");

    st_ut_eq(-1,   s.type,        "type is -1");
    st_ut_eq(0,    s.len,         "len is 0");
    st_ut_eq(0,    s.capacity,    "capacity is 0");
    st_ut_eq(0,    s.bytes_owned, "bytes_owned is 0");
    st_ut_eq(NULL, s.bytes,       "bytes is NULL");
}

st_test(str, ref) {

    {
        st_str_t s = st_str_null;
        st_str_t t = st_str_null;

        st_ut_eq(ST_ARG_INVALID, st_str_ref(NULL, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_ref(NULL, &s), "");
        st_ut_eq(ST_ARG_INVALID, st_str_ref(&s, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_ref(&s, &s), "");

        s.bytes_owned = 1;
        st_ut_eq(ST_INITTWICE, st_str_ref(&s, &t), "");
    }

    int ret;

    st_str_t s = st_str_null;
    ret = st_str_init(&s, 10);
    st_ut_eq(ST_OK, ret, "init ok");

    st_str_t ref = st_str_null;

    ret = st_str_ref(&ref, &s);
    st_ut_eq(ST_OK, ret, "");

    st_ut_eq(1, s.bytes_owned, "target bytes_owned does not change");

    st_ut_eq(0,       ref.type,        "type is 0");
    st_ut_eq(10,      ref.len,         "len is 0");
    st_ut_eq(10,      ref.capacity,    "capacity is 0");
    st_ut_eq(0,       ref.bytes_owned, "bytes_owned is 0, ref does not need to free memory");
    st_ut_eq(s.bytes, ref.bytes,       "bytes is shared");

    ret = st_str_destroy(&s);
    st_ut_eq(ST_OK, ret, "destroy s");

    ret = st_str_destroy(&ref);
    st_ut_eq(ST_OK, ret, "destroy ref");
}

st_test(str, copy) {

    {
        st_str_t s = st_str_null;
        st_str_t t = st_str_null;

        st_ut_eq(ST_ARG_INVALID, st_str_copy(NULL, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_copy(NULL, &s), "");
        st_ut_eq(ST_ARG_INVALID, st_str_copy(&s, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_copy(&s, &s), "");

        s.bytes_owned = 1;
        st_ut_eq(ST_INITTWICE, st_str_ref(&s, &t), "");
    }

    int ret;

    st_str_t s = st_str_null;
    ret = st_str_init_0(&s, 10);
    st_ut_eq(ST_OK, ret, "init ok");

    st_str_t cpy = st_str_null;

    ret = st_str_copy(&cpy, &s);
    st_ut_eq(ST_OK, ret, "");

    st_ut_eq(1, s.bytes_owned, "target bytes_owned does not change");

    st_ut_eq(0,          cpy.type,        "type is 0");
    st_ut_eq(s.len,      cpy.len,         "len is same as s");
    st_ut_eq(s.capacity, cpy.capacity,    "capacity is same as s");
    st_ut_eq(1,          cpy.bytes_owned, "bytes_owned is 1, memory reallocated");
    st_ut_ne(s.bytes,    cpy.bytes,       "bytes is not shared");

    ret = st_memcmp(s.bytes, cpy.bytes, s.capacity);
    st_ut_eq(0, ret, "cpy.bytes is same as s.bytes");

    ret = st_str_destroy(&s);
    st_ut_eq(ST_OK, ret, "destroy s");

    ret = st_str_destroy(&cpy);
    st_ut_eq(ST_OK, ret, "destroy cpy");
}

st_test(str, copy_cstr) {

    {
        st_str_t s = st_str_null;

        st_ut_eq(ST_ARG_INVALID, st_str_copy_cstr(NULL, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_copy_cstr(NULL, FOO), "");
        st_ut_eq(ST_ARG_INVALID, st_str_copy_cstr(&s, NULL), "");

        s.bytes_owned = 1;
        st_ut_eq(ST_INITTWICE, st_str_copy_cstr(&s, FOO), "");
    }

    int ret;

    st_str_t s = st_str_null;

    ret = st_str_copy_cstr(&s, FOO);
    st_ut_eq(ST_OK, ret, "");

    st_ut_eq(0,             s.type,        "type is 0");
    st_ut_eq(sizeof(FOO)-1, s.len,         "len is strlen(FOO)");
    st_ut_eq(sizeof(FOO),   s.capacity,    "capacity is sizeof(FOO)");
    st_ut_eq(1,             s.bytes_owned, "bytes_owned is 1, memory allocated");
    st_ut_ne(NULL,          s.bytes,       "bytes is not NULL");

    ret = st_memcmp(FOO, s.bytes, s.len);
    st_ut_eq(0, ret, "s.bytes is same as FOO");
    st_ut_eq('\0', s.bytes[s.len], "s has trailing 0");

    ret = st_str_destroy(&s);
    st_ut_eq(ST_OK, ret, "destroy s");
}

st_test(str, seize) {

    {
        st_str_t s = st_str_null;
        st_str_t t = st_str_null;

        st_ut_eq(ST_ARG_INVALID, st_str_seize(NULL, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_seize(NULL, &s), "");
        st_ut_eq(ST_ARG_INVALID, st_str_seize(&s, NULL), "");
        st_ut_eq(ST_ARG_INVALID, st_str_seize(&s, &s), "");

        s.bytes_owned = 1;
        st_ut_eq(ST_INITTWICE, st_str_ref(&s, &t), "");
    }

    int ret;

    st_str_t s = st_str_null;
    ret = st_str_init_0(&s, 10);
    st_ut_eq(ST_OK, ret, "init ok");

    st_str_t seize = st_str_null;

    ret = st_str_seize(&seize, &s);
    st_ut_eq(ST_OK, ret, "");

    st_ut_eq(0, s.bytes_owned, "target bytes_owned does not change");

    st_ut_eq(s.type,     seize.type,        "type is same as s");
    st_ut_eq(s.len,      seize.len,         "len is same as s");
    st_ut_eq(s.capacity, seize.capacity,    "capacity is same as s");
    st_ut_eq(1,          seize.bytes_owned, "bytes_owned is 1");
    st_ut_eq(s.bytes,    seize.bytes,       "bytes is copied");

    ret = st_str_destroy(&s);
    st_ut_eq(ST_OK, ret, "destroy s");

    ret = st_str_destroy(&seize);
    st_ut_eq(ST_OK, ret, "destroy seize");
}

/* TODO test n_arr */
st_ut_main;
