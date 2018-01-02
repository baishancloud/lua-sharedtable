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

typedef struct st_table_gc_s st_table_gc_t;
typedef struct st_table_element_s st_table_element_t;
typedef struct st_table_s st_table_t;
typedef struct st_table_pool_s st_table_pool_t;

typedef int (*st_table_visit_f)(st_table_element_t *elem, void *data);

#define ST_TABLE_UNTRACKED 0
#define ST_TABLE_REACHABLE 1
#define ST_TABLE_UNREACHABLE 2

#define ST_TABLE_GC_COUNT_THRESHOLD 1024
#define ST_TABLE_GC_TIME_THRESHOLD (6 * 3600)

#define ST_TABLE_VALUE_TYPE_LEN (int64_t)sizeof(st_table_value_type_t)

typedef enum st_table_value_type_e {
    ST_TABLE_VALUE_TYPE_TABLE = 0x01,
    //TODO shulong add other types
    _ST_TABLE_VALUE_TYPE_MAX,
} st_table_value_type_t;

/**
 * each table contain gc struct, it is used to track table,
 * identify wether table has been circular reference.
 */
struct st_table_gc_s {
    /* used for table_pool table_list*/
    st_list_t lnode;

    /* it is copy of the table refcnt when in gc runtime */
    int64_t refcnt;

    /* table state, value is ST_TABLE_UNTRACKED, ST_TABLE_REACHABLE, ST_TABLE_UNREACHABLE */
    int state;
};

struct st_table_element_s {
    /* used for table rbtree */
    st_rbtree_node_t rbnode;

    /* key used to identity value, key is no type, user should interpret it */
    st_str_t key;

    /**
     * value contain type and user data
     * front ST_TABLE_VALUE_TYPE_LEN bytes store value type and next bytes store user data.
     * value => | TYPE | USER VALUE |
     */
    st_str_t value;

    /* space to store key and value */
    uint8_t kv_data[0];
};

struct st_table_s {
    st_table_pool_t *pool;

    st_table_gc_t gc;

    /* all table elements are stored in rbtree */
    st_rbtree_t elements;

    /* the lock protect elements rbtree */
    pthread_mutex_t lock;

    /**
     * 1. new table, refcnt is 1.
     * 2. get table reference, refcnt increase 1.
     * 3. x table is inserted into y table, x table refcnt increase 1.
     *
     * 4. release table, refcnt decrease 1.
     * 5. return table reference, refcnt decrease 1.
     * 6. x table is removed from y table, x table refcnt decrease 1.
     */
    int64_t refcnt;

    int inited;
};

struct st_table_pool_s {
    st_slab_pool_t slab_pool;

    /**
     * if you make sure that will not create circular reference table.
     * you should disable gc, because it can loss performance.
     */
    int enable_gc;

    /* gc run start time */
    time_t gc_start_tm;

    /* current tables count */
    int64_t table_count;

    /**
     * created tables will be inserted into table_list
     * destroied tables will be removed from table_list
     * the list only used when enable_gc is true
     */
    st_list_t table_list;

    /**
     * pool global lock, protect situation:
     * 1. protect table_list
     * 2. protect gc_start_tm
     * 3. protect table_count
     * 4. protect in gc runtime no other table refcnt will be handled
     */
    pthread_mutex_t lock;
};

static inline st_table_value_type_t st_table_get_value_type(st_str_t value) {
    return *(st_table_value_type_t *)(value.bytes);
}

static inline int st_table_value_is_table(st_str_t value) {
    return st_table_get_value_type(value) == ST_TABLE_VALUE_TYPE_TABLE;
}

static inline st_table_t *st_table_get_table_addr_from_value(st_str_t value) {
    return *(st_table_t **)(value.bytes + ST_TABLE_VALUE_TYPE_LEN);
}

/* new a table, and it's refcnt is set to 1 */
int st_table_new(st_table_pool_t *pool, st_table_t **table);

/* the function decrease table refcnt, if refcnt is 0, table space will be released */
int st_table_release(st_table_t *table);

/**
 * if a process want to use the table,
 * first it should use the function to get table reference.
 * then you will not afraid other process release the table.
 */

int st_table_incref(st_table_t *table);

/**
 * if you have got the table reference,
 * you must call the function to return the table reference.
 * if you don't , table will hold refcnt, and the table space will never be released.
 */
int st_table_decref(st_table_t *table);

/**
 * you can add user value or other table reference into table.
 * if you add table, the table refcnt will be increased 1.
 */
int st_table_add_value(st_table_t *table, st_str_t key, st_str_t value);

/**
 * remove value from table.
 * if value is table reference, the table refcnt will be decreased 1.
 */
int st_table_remove_value(st_table_t *table, st_str_t key);

/**
 * you can find value in table.
 * the function is no locked, because user will copy or do other thing in his code
 * so lock the table first.
 */
int st_table_get_value(st_table_t *table, st_str_t key, st_str_t *value);

/**
 * you can find bigger value in table.
 * the function is no locked, because user will copy or do other thing in his code
 * so lock the table first.
 */
int st_table_get_bigger_value(st_table_t *table, st_str_t key, st_str_t *value);

/**
 * get sub table from the table, and the sub table refcnt will be increased 1.
 * the function is locked.
 */
int st_table_get_table_withref(st_table_t *table, st_str_t key, st_table_t **sub);

/* clear all the elements in table */
int st_table_clear(st_table_t *table);

/**
 * clear the table circular reference, it will be automatic called in new table step.
 * you can call the function explicitly.
 */
int st_table_clear_circular_ref(st_table_pool_t *pool, int force);

int st_table_pool_init(st_table_pool_t *pool, int enable_gc);

int st_table_pool_destroy(st_table_pool_t *pool);

#endif /* _TABLE_H_INCLUDED_ */
