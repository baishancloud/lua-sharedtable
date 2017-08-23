#ifndef __INC__UTIL_H__
#     define __INC__UTIL_H__

#include <signal.h>
#include <immintrin.h>

/* TODO  */
/* #define st_pause() __asm__("pause\n") */
#define st_pause() _mm_pause()

#define st_typeof(x) __typeof__(x)
#define st_nelts(arr) (sizeof(arr) / sizeof(arr[0]))

#define st_unused(x) (void)(x)

/* upto must be powers of 2 */
#define st_align(i, upto)                                                    \
        ( (upto) == 0 ?                                                       \
          (i) :                                                               \
          ( ((i) + (upto) - 1)                                                \
            & ( ~((upto) - 1) )                                               \
          )                                                                   \
        )

#define st_offsetof(tp, field) ( (size_t)&((tp*)NULL)->field )
#define st_by_offset(p, offset) &((char*)(p))[ (offset) ]
#define st_owner(p, tp, field) ((void*)(st_by_offset(p, - st_offsetof(tp, field))))

#define st_min(a, b) ( (a) < (b) ? (a) : (b) )
#define st_max(a, b) ( (a) > (b) ? (a) : (b) )

#define st_return_if(cond, val)                                              \
        do {                                                                  \
            if (cond) {                                                     \
                return (val);                                                 \
            }                                                                 \
        } while (0);

#define st_bug(_fmt, ...) raise(SIGABRT)


#define st_arg1(a, ...) a
#define st_arg2(__, a, ...) a
#define st_arg23(__, a, b, ...) a, b
#define st_arg3(a1, a2, a3, ...) a3
#define st_shift(a, ...) __VA_ARGS__

/* macro-for-each */

#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__

#define __empty_ 1
/* #define is_empty(ph, ...) arg3(ph, ##__VA_ARGS__, 1, 0) */
#define is_empty(ph, ...) ph, ##__VA_ARGS__, 1

#define empty()
#define defer(id) id empty()
#define obstruct(...) __VA_ARGS__ defer(empty)()

#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0

#define IIF(cond) CAT(IIF_,cond)
#define IIF_0(t, f) f
#define IIF_1(t, f) t

#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 PROBE(~)

#define BOOL(x) COMPL(NOT(x))
#define IF(c) IIF(BOOL(c))

#define CHECK_N(x, n, ...) n
#define CHECK(...) CHECK_N(__VA_ARGS__, 0,)
#define PROBE(x) x, 1,

#define EAT(...)
#define EXPAND(...) __VA_ARGS__
#define WHEN(c) IF(c)(EXPAND, EAT)

#define IS_PAREN(x) CHECK(IS_PAREN_PROBE x)
#define IS_PAREN_PROBE(...) PROBE(~)

#define no_args_x(...) #__VA_ARGS__
#define no_args(...) 0 EVAL(no_args_(__VA_ARGS__, ()))

#define no_args_(c, ...) \
        IIF(BOOL(IS_PAREN(c)))( , \
        + 1 \
        obstruct(no_args_xx)()(__VA_ARGS__) \
)

#define no_args_xx() no_args_

#define st_foreach(statement, ...) EVAL(foreach(statement, __VA_ARGS__, ~))

#define foreach(statement, c, ...) \
        IIF(BOOL(IS_PAREN(c)))( \
        statement c \
        obstruct(foreach_xx)()(statement, __VA_ARGS__) \
, \
)

#define foreach_xx() foreach

/* argument checking */

#define st_must(expr, ...) do {                                              \
      if (! (expr)) {                                                         \
          derr("arg check fail: "#expr);                                      \
          return (void)0, ##__VA_ARGS__;                                      \
      }                                                                       \
  } while (0)

#define st_assert(expr) do {                                                 \
      if (! (expr)) {                                                         \
          derr("assertion fail: "#expr);                                      \
          st_bug("assertion fail: "#expr);                                   \
      }                                                                       \
  } while (0)

#ifdef ST_DEBUG
#   define st_dmust(expr, ...) st_must(expr, ##__VA_ARGS__)
#   define st_dassert(expr) st_assert(expr)
#else
#   define st_dmust(expr, ...)
#   define st_dassert(expr)
#endif /* ST_DEBUG */

#endif /* __INC__UTIL_H__ */
