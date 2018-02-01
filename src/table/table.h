#ifndef _TABLE_H_INCLUDED_
#define _TABLE_H_INCLUDED_

#include <stdint.h>
#include <string.h>
#include <time.h>

#include "inc/inc.h"

#include "atomic/atomic.h"
#include "list/list.h"
#include "rbtree/rbtree.h"
#include "robustlock/robustlock.h"
#include "slab/slab.h"
#include "str/str.h"
#include "gc/gc.h"

typedef struct st_table_element_s st_table_element_t;
typedef st_table_element_t *st_table_iter_t;

typedef struct st_table_s st_table_t;
typedef struct st_table_pool_s st_table_pool_t;

typedef int (*st_table_visit_f)(st_str_t value, void *arg);

struct st_table_element_s {
    // used for table rbtree
    st_rbtree_node_t rbnode;

    st_str_t key;
    st_str_t value;

    /* space to store key and value */
    uint8_t kv_data[0];
};

struct st_table_s {
    // used for gc
    st_gc_head_t gc_head;

    st_table_pool_t *pool;

    // all table elements are stored in rbtree
    st_rbtree_t elements;

    int64_t element_cnt;

    // the lock protect elements rbtree
    pthread_mutex_t lock;

    int inited;
};

struct st_table_pool_s {
    st_slab_pool_t slab_pool;

    st_gc_t gc;

    int run_gc_periodical;

    // current tables cnt
    int64_t table_cnt;
};

static inline void st_table_init_iter(st_table_iter_t *iter) {
    *iter = NULL;
}

static inline st_table_t *st_table_get_table_addr_from_value(st_str_t value) {
    return *(st_table_t **)(value.bytes);
}

int st_table_new(st_table_pool_t *pool, st_table_t **table);

int st_table_release(st_table_t *table);

// this function is only used for gc, other one please use st_table_clear.
int st_table_remove_all_for_gc(st_table_t *table);

int st_table_remove_all(st_table_t *table);

// traverse table each element, and call visit_func for element value.
int st_table_foreach(st_table_t *table, st_table_visit_f visit_func, void *arg);

int st_table_add_key_value(st_table_t *table, st_str_t key, st_str_t value);

int st_table_remove_key(st_table_t *table, st_str_t key);

// you can find value in table.
// the function is no locked, because user will copy or do other thing in his code
// so lock the table first.
int st_table_get_value(st_table_t *table, st_str_t key, st_str_t *value);

// you can find next value in table, it will be used for iterating table.
// the function is no locked, because user will copy or do other thing in his code
// so lock the table first.
int st_table_iter_next_value(st_table_t *table, st_table_iter_t *iter, st_str_t *value);

int st_table_pool_init(st_table_pool_t *pool, int run_gc_periodical);

int st_table_pool_destroy(st_table_pool_t *pool);

#endif /* _TABLE_H_INCLUDED_ */
