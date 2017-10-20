#ifndef __UNITTEST__UNITTEST_H__
#define __UNITTEST__UNITTEST_H__

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>

#include "inc/log.h"
#include "inc/util.h"

#ifdef ST_DEBUG_UNITTEST
#   define OK_OUT( _fmt, ... ) st_log_printf( _fmt "\n", "[TEST]", ##__VA_ARGS__ )
#   define BENCH_DEBUG( _fmt, ... ) st_log_printf( _fmt "\n", "[BENCH]", ##__VA_ARGS__ )
#   define st_ut_debug( _fmt, ... ) st_log_printf( _fmt "\n", "[UT-DEBUG]", ##__VA_ARGS__ )
#else
#   define OK_OUT( ... )
#   define BENCH_DEBUG( _fmt, ... )
#   define st_ut_debug( _fmt, ... )
#endif /* ST_DEBUG_UNITTEST */

#define ERR_OUT( _fmt, ... ) st_log_printf( _fmt "\n", "[TEST]", ##__VA_ARGS__ )

#define BENCH_INFO( _fmt, ... ) st_log_printf( _fmt "\n", "[BENCH]", ##__VA_ARGS__ )

#define st_pointer_t "%p"
#define st_pointer void*

typedef struct st_ut_case_t st_ut_case_t;
typedef struct st_ut_bench_t st_ut_bench_t;

extern st_ut_case_t *st_ut_cases_;
extern int64_t       st_ut_case_n_;
extern int64_t       st_ut_case_capacity_;
extern int           st_ut_ret__;

extern st_ut_bench_t *st_ut_benches_;
extern int64_t        st_ut_bench_n_;
extern int64_t        st_ut_bench_capacity_;
extern int            st_ut_ret__;

typedef void (*st_ut_case_f)();

typedef void (*st_ut_bench_f)(void *data, int64_t n);
typedef void *(*st_ut_bench_init_f)();
typedef void (*st_ut_bench_clean_f)(void *data);

static inline int main_(int argc, char **argv);

static inline void st_ut_add_case(char *fname, st_ut_case_f func);

static inline int st_ut_is_bench(int argc, char **argv);
static inline void st_ut_add_bench(char *fname,
                                    st_ut_bench_f func,
                                    st_ut_bench_init_f init,
                                    st_ut_bench_clean_f clean,
                                    int64_t n_sec);
static inline void
st_ut_run_bench(st_ut_bench_t *b);


char *st_ut_get_tostring_buf();


/* define test */
#define st_test(case_, func_)                                                 \
    static void st_ut_case_##case_##_##func_();                               \
    __attribute__((constructor))                                              \
    void st_ut_case_##case_##_##func_##_constructor_() {                      \
        st_ut_add_case(#case_ "::" #func_, st_ut_case_##case_##_##func_);     \
    }                                                                         \
    static void st_ut_case_##case_##_##func_()

/* define benchmark */
#define st_ben(name_, func_, n_sec) \
        st_bench(name_, func_, NULL, NULL, n_sec)

#define st_bench(name_, func_, init_func, clean_func, n_sec)                  \
    static void st_ut_bench_##name_##_##func_(void*data, int64_t n);          \
    __attribute__((constructor))                                              \
    void st_ut_bench_##name_##_##func_##_constructor_() {                     \
        st_ut_add_bench(#name_ "::" #func_, st_ut_bench_##name_##_##func_,    \
                         init_func, clean_func, n_sec);                       \
    }                                                                         \
    static void st_ut_bench_##name_##_##func_(void*data, int64_t n)

#define st_ut_return_fail()                                                   \
        do {                                                                  \
            if (st_ut_does_fail()) {                                          \
                return;                                                       \
            }                                                                 \
        } while(0)

#define st_ut_does_fail() (st_ut_ret__ != 0)

/* assertions */
#define st_ut_eq(expected, actual, fmt, ...)                                    \
    st_ut_assert_cmp_(==, expected, actual, fmt, ##__VA_ARGS__)

#define st_ut_ne(expected, actual, fmt, ... )                                   \
    st_ut_assert_cmp_(!=, expected, actual, fmt, ##__VA_ARGS__)

#define st_ut_ge(expected, actual, fmt, ...)                                    \
    st_ut_assert_cmp_(>=, expected, actual, fmt, ##__VA_ARGS__)

#define st_ut_gt(expected, actual, fmt, ...)                                    \
    st_ut_assert_cmp_(>,  expected, actual, fmt, ##__VA_ARGS__)

#define st_ut_le(expected, actual, fmt, ...)                                    \
    st_ut_assert_cmp_(<=, expected, actual, fmt, ##__VA_ARGS__)

#define st_ut_lt(expected, actual, fmt, ...)                                    \
    st_ut_assert_cmp_(<,  expected, actual, fmt, ##__VA_ARGS__)

#define st_ut_assert_cmp_(_operator, __e, __a, fmt, ...)                     \
    do {                                                                      \
        st_typeof(__e) _e = (__e); \
        st_typeof(__a) _a = (__a); \
        int __rst = st_ut_compare_func_(_e)(_e, _a);                      \
        st_ut_assert_(__rst _operator 0,                                     \
             "Expected: '%s' " #_operator " '%s' " fmt,                       \
             st_ut_print_func_(_e)(_e),                                    \
             st_ut_print_func_(_e)(_a),                                    \
             ##__VA_ARGS__ );                                                 \
    } while (0)

#define st_ut_assert_( to_be_true, mes, ... )                                \
    do {                                                                      \
        if ( (to_be_true) ) {                                                 \
            OK_OUT( mes, ##__VA_ARGS__ );                                     \
            st_ut_ret__ = 0;                                                 \
        }                                                                     \
        else {                                                                \
            ERR_OUT( mes, ##__VA_ARGS__ );                                    \
            st_ut_ret__ = -1;                                                \
            return;                                                           \
        }                                                                     \
    } while ( 0 )

struct st_ut_case_t {
    char *name;
    st_ut_case_f func;
};
struct st_ut_bench_t {
    char *name;
    int64_t n_sec;
    st_ut_bench_f func;
    st_ut_bench_init_f init;
    st_ut_bench_clean_f clean;
};

static inline void
st_ut_run_bench(st_ut_bench_t *b) {
    int            rc;
    struct timeval t0;
    struct timeval t1;
    int64_t        spent_usec;
    void          *data;

    for (int64_t n = 64; n > 0 /* overflow */; ) {
        data = NULL;
        if (b->init != NULL) {
            data = b->init(n);
        }

        gettimeofday(&t0, NULL);

        printf("benching %s n=%lld:\n", b->name, n);
        b->func(data, n);

        gettimeofday(&t1, NULL);

        if (b->clean != NULL) {
            b->clean(data);
        }
        spent_usec = (t1.tv_sec - t0.tv_sec);
        spent_usec = spent_usec*1000*1000 + (t1.tv_usec - t0.tv_usec);

        BENCH_DEBUG("%s cost: %ld ms, n=%llu", b->name, spent_usec/1000, n);
        if (spent_usec > b->n_sec * 1000 * 1000) {
            double ns_per_call = (double)spent_usec * 1000 / n;
            BENCH_INFO(
                "%s runs"
                "    %llu times\n"
                "    costs %lld mseconds\n"
                "    calls/s: %llu\n"
                "    ns/call(10e-9): %.3f",
                b->name, n, spent_usec / 1000,
                n * 1000 * 1000 / spent_usec, ns_per_call);
            break;
        } else {
            if (spent_usec > 10*1000) {
                n = b->n_sec * 1000 * 1000 / spent_usec * n * 3/2;
            } else {
                n = n * 4;
            }
        }
    }
}

#define st_ut_compare_func_(v) _Generic((v),                                 \
        char:       st_cmp_char,                                             \
        int8_t:     st_cmp_int8,                                             \
        int16_t:    st_cmp_int16,                                            \
        int32_t:    st_cmp_int32,                                            \
        int64_t:    st_cmp_int64,                                            \
        uint8_t:    st_cmp_uint8,                                            \
        uint16_t:   st_cmp_uint16,                                           \
        uint32_t:   st_cmp_uint32,                                           \
        uint64_t:   st_cmp_uint64,                                           \
        default:    st_cmp_void_ptr,                                         \
        void*:      st_cmp_void_ptr                                          \
)

#define st_ut_print_func_(v) _Generic((v),                                   \
                                       char:       st_print_char,            \
                                       int8_t:     st_print_int8,            \
                                       int16_t:    st_print_int16,           \
                                       int32_t:    st_print_int32,           \
                                       int64_t:    st_print_int64,           \
                                       uint8_t:    st_print_uint8,           \
                                       uint16_t:   st_print_uint16,          \
                                       uint32_t:   st_print_uint32,          \
                                       uint64_t:   st_print_uint64,          \
                                       default:    st_print_void_ptr,        \
                                       void*:      st_print_void_ptr         \
)


#define st_ut_norm_cmp(a, b) ((a)>(b) ? 1 : (((a)<(b)) ? -1 : 0))

static inline int st_cmp_char(char      a, char      b) { return st_ut_norm_cmp(a, b); }

static inline int st_cmp_int8(int8_t    a, int8_t    b) { return st_ut_norm_cmp(a, b); }
static inline int st_cmp_int16(int16_t   a, int16_t   b) { return st_ut_norm_cmp(a, b); }
static inline int st_cmp_int32(int32_t   a, int32_t   b) { return st_ut_norm_cmp(a, b); }
static inline int st_cmp_int64(int64_t   a, int64_t   b) { return st_ut_norm_cmp(a, b); }

static inline int st_cmp_uint8(uint8_t   a, uint8_t   b) { return st_ut_norm_cmp(a, b); }
static inline int st_cmp_uint16(uint16_t  a, uint16_t  b) { return st_ut_norm_cmp(a, b); }
static inline int st_cmp_uint32(uint32_t  a, uint32_t  b) { return st_ut_norm_cmp(a, b); }
static inline int st_cmp_uint64(uint64_t  a, uint64_t  b) { return st_ut_norm_cmp(a, b); }

static inline int st_cmp_void_ptr(void  *a, void *b) { return st_ut_norm_cmp(a, b); }

#define st_ut_print_(fmt, v) do { \
        char *__buf = st_ut_get_tostring_buf(); \
        snprintf(__buf, 64, fmt, v); \
        return __buf; \
    } while(0)

static inline char *st_print_char(char      a) { st_ut_print_("%c",   a); }

static inline char *st_print_int8(int8_t    a) { st_ut_print_("%d",   a); }
static inline char *st_print_int16(int16_t   a) { st_ut_print_("%d",   a); }
static inline char *st_print_int32(int32_t   a) { st_ut_print_("%d",   a); }
static inline char *st_print_int64(int64_t   a) { st_ut_print_("%lld", a); }

static inline char *st_print_uint8(uint8_t   a) { st_ut_print_("%u",   a); }
static inline char *st_print_uint16(uint16_t  a) { st_ut_print_("%u",   a); }
static inline char *st_print_uint32(uint32_t  a) { st_ut_print_("%u",   a); }
static inline char *st_print_uint64(uint64_t  a) { st_ut_print_("%llu", a); }

static inline char *st_print_void_ptr(void *p) { st_ut_print_("%p", p); }

static inline int st_ut_is_bench(int argc, char **argv) {
    return (argc >= 2)
           && (sizeof("bench") - 1 == strlen(argv[1]))
           && (0 == memcmp(argv[1], "bench", sizeof("bench") - 1));
}

static inline void st_ut_add_case(char *fname, st_ut_case_f func) {
    if (st_ut_cases_ == NULL) {
        st_ut_case_capacity_ = 1;
        st_ut_cases_ = malloc(sizeof(*st_ut_cases_) * st_ut_case_capacity_);
        st_ut_case_n_ = 0;
    }

    while (st_ut_case_n_ >= st_ut_case_capacity_) {
        st_ut_case_capacity_ *= 4;
        st_ut_cases_ = realloc(st_ut_cases_,
                                sizeof(*st_ut_cases_) * st_ut_case_capacity_);
    }

    st_ut_cases_[st_ut_case_n_].name = fname;
    st_ut_cases_[st_ut_case_n_].func = func;
    st_ut_case_n_++;
}

static inline void st_ut_add_bench(char *fname,
                                    st_ut_bench_f func,
                                    st_ut_bench_init_f init,
                                    st_ut_bench_clean_f clean,
                                    int64_t n_sec) {
    if (st_ut_benches_ == NULL) {
        st_ut_bench_capacity_ = 1;
        st_ut_benches_ = malloc(sizeof(*st_ut_benches_) * st_ut_bench_capacity_);
        st_ut_bench_n_ = 0;
    }

    while (st_ut_bench_n_ >= st_ut_bench_capacity_) {
        st_ut_bench_capacity_ *= 4;
        st_ut_benches_ = realloc(st_ut_benches_,
                                  sizeof(*st_ut_benches_) * st_ut_bench_capacity_);
    }

    st_ut_bench_t *b = &st_ut_benches_[st_ut_bench_n_];
    b->name = fname;
    b->func = func;
    b->init = init;
    b->clean = clean;
    b->n_sec = n_sec ? : 1;
    st_ut_bench_n_++;
}

static inline int main_(int argc, char **argv) {

    /* test */
    for (int i = 0; i < st_ut_case_n_; i++) {
        st_ut_cases_[i].func();
        if (st_ut_ret__ == -1) {
            printf("test fail: %s\n", st_ut_cases_[i].name);
            return -1;
        }
        printf("test   ok: %s\n", st_ut_cases_[i].name);
    }
    printf("all passed, n_cases = %lld\n", st_ut_case_n_);

    /* bench */
    if (st_ut_is_bench(argc, argv)) {

        for (int i = 0; i < st_ut_bench_n_; i++) {

            st_ut_bench_t *b = &st_ut_benches_[i];
            printf("bench: %s\n", b->name);
            st_ut_run_bench(b);
        }
        printf("benches done:\n");
    }

    return 0;
}

#define st_ut_main                                                           \
    st_ut_case_t *st_ut_cases_;                                             \
    int64_t st_ut_case_n_;                                                   \
    int64_t st_ut_case_capacity_;                                            \
    int st_ut_ret__;                                                         \
    st_ut_bench_t *st_ut_benches_;                                          \
    int64_t st_ut_bench_n_;                                                  \
    int64_t st_ut_bench_capacity_;                                           \
    char *st_ut_get_tostring_buf() {                                         \
        static __thread char buffers[128][64];                                \
        static __thread uint64_t i = 0;                                       \
        char *buffer = buffers[i++ % 128];                                    \
        return buffer;                                                        \
    }                                                                         \
    int main(int argc, char **argv) {                                         \
        return main_(argc, argv);                                             \
    }

#endif /* __UNITTEST__UNITTEST_H__ */
