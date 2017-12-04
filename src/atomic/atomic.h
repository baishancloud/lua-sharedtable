#ifndef st_atomic_
#define st_atomic_

#include "inc/util.h"

#define ST_ATOMIC_STRONGEST __ATOMIC_SEQ_CST

/*
 * memory barrior level is optional
 *
 * __ATOMIC_RELAXED
 * No barriers or synchronization.
 * __ATOMIC_CONSUME
 * Data dependency only for both barrier and synchronization with another
 * thread.
 *
 * __ATOMIC_ACQUIRE
 * Barrier to hoisting of code and synchronizes with release (or stronger)
 * semantic stores from another thread.
 *
 * __ATOMIC_RELEASE
 * Barrier to sinking of code and synchronizes with acquire (or stronger)
 * semantic loads from another thread.
 *
 * __ATOMIC_ACQ_REL
 * Full barrier in both directions and synchronizes with acquire loads and
 * release stores in another thread.
 *
 * __ATOMIC_SEQ_CST
 * Full barrier in both directions and synchronizes with acquire loads and
 * release stores in all threads.
 */

#define st_atomic_default_(...) st_arg2(__VA_ARGS__, ST_ATOMIC_STRONGEST)
#define st_atomic_default2_(...) st_arg23(__VA_ARGS__, ST_ATOMIC_STRONGEST, ST_ATOMIC_STRONGEST)

#define st_atomic_load(p, ...) __atomic_load_n((p), st_atomic_default_(, ## __VA_ARGS__))
#define st_atomic_store(p, val, ...) __atomic_store_n((p), (val), st_atomic_default_(, ## __VA_ARGS__))

#define st_atomic_fetch_add(p, val, ...) __atomic_fetch_add((p), (val), st_atomic_default_(, ## __VA_ARGS__))
#define st_atomic_add(p, val, ...) __atomic_add_fetch((p), (val), st_atomic_default_(, ## __VA_ARGS__))

#define st_atomic_fetch_sub(p, val, ...) __atomic_fetch_sub((p), (val), st_atomic_default_(, ## __VA_ARGS__))
#define st_atomic_sub(p, val, ...) __atomic_sub_fetch((p), (val), st_atomic_default_(, ## __VA_ARGS__))

#define st_atomic_swap(p, val, ...) __atomic_exchange_n((p), (val), st_atomic_default_(, ## __VA_ARGS__))

#define st_atomic_incr(p, val, ...) __atomic_add_fetch(p, val, st_atomic_default_(, ## __VA_ARGS__))
#define st_atomic_decr(p, val, ...) __atomic_sub_fetch(p, val, st_atomic_default_(, ## __VA_ARGS__))

#define st_atomic_cas(p, expected_p, val, ...) \
        __atomic_compare_exchange_n((p), (expected_p), (val), \
                                    0, st_atomic_default2_(, ##__VA_ARGS__))

#endif /* st_atomic_ */
