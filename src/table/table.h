#ifndef _TABLE_H_INCLUDED_
#define _TABLE_H_INCLUDED_

#include <stdint.h>
#include <string.h>

#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"
#include "inc/callback.h"

#include "str/str.h"
#include "rbtree/rbtree.h"
#include "list/list.h"
#include "robustlock/robustlock.h"
#include "slab/slab.h"
#include "refcnt/refcnt.h"

typedef struct st_table_element_s st_table_element_t;
typedef struct st_table_s st_table_t;
typedef struct st_table_pool_s st_table_pool_t;

#define ST_TABLE_MAX_NAME_LEN 512

/**
 * table element, store key, value.
 */

struct st_table_element_s {
    /* used for table rbtree */
    st_rbtree_node_t rbnode;

    /* used for iterate table all elements, put them into a list */
    st_list_t lnode;

    /* key used to identity value, key is no type, user should interpret it */
    st_str_t key;

    /**
     * there are two type value:
     * 1. user interpret type, user put type and value into val,
     *    user can specify it as int, double, string etc.
     * 2. table reference type, table address is stored in val.
     */
    st_str_t val;

    /* identity whether val is table reference */
    int is_table_ref;

    /* memory space to store key and val */
    uint8_t kv_data[0];
};

struct st_table_s {
    st_table_pool_t *pool;

    /* all table elements in rbtree */
    st_rbtree_t elements;

    /**
     * lock only protect elements rbtree,
     * to maximize the table concurrency,
     * so alloc or free memory should be called outside of the lock
     */
    pthread_mutex_t lock;

    /* table name */
    st_str_t name;
    /* memory space for name */
    uint8_t name_data[ST_TABLE_MAX_NAME_LEN];

    /**
     * table refcnt, if a process get table ref,
     * or table is inserted into another table,
     * the refcnt will increase.
     */
    st_refcnt_t refcnt;

    int inited;
};

struct st_table_pool_s {
    /**
     * all user created table will be added into root_table,
     * the element key is table name
     */
    st_table_t root_table;

    st_slab_pool_t slab_pool;
};

int st_table_pool_init(st_table_pool_t *pool);

int st_table_pool_destroy(st_table_pool_t *pool);

/**
 * new table will be added into root_table,
 * if table already exist, ST_EXISTED will return.
 */
int st_table_new(st_table_pool_t *pool, st_str_t name, st_table_t **new);

/**
 * released table will be removed from root_table,
 * if table refcnt is 0, table space will be release.
 */
int st_table_release(st_table_t *table);

/**
 * if you havn't new table,
 * you should call the function to get table reference.
 * then you can use the table.
 */
int st_table_get_ref(st_table_pool_t *pool, st_str_t name, st_table_t **table);

/**
 * if you don't need the table, you must call the function to return the table ref.
 * if you don't , table will hold refcnt, and the table space will never be released.
 */
int st_table_return_ref(st_table_t *table);

/**
 * you can insert user value or other table reference into table.
 * if you insert table reference, the table refcnt will be increased 1.
 */
int st_table_insert_value(st_table_t *table, st_str_t key, st_str_t val, int is_table_ref,
                          int force);

/**
 * remove value from table.
 * if value is table reference, the table refcnt will be decreased 1.
 */
int st_table_remove_value(st_table_t *table, st_str_t key);

/**
 * lock table. if you call st_table_get_value or st_table_get_all_elements function
 * you must lock the table, then you can copy value or do other thing.
 */
int st_table_lock(st_table_t *table);

/**
 * you can find value in table.
 * the function is no locked, because user will copy or do other thing in his code
 * so called st_table_lock first.
 */
int st_table_get_value(st_table_t *table, st_str_t key, st_str_t *value);

/**
 * you can iterate all elements from table, the elements will be inserted into list.
 * the function is no locked, because user will copy or do other thing in his code
 * so called st_table_lock first.
 */
int st_table_get_all_elements(st_table_t *table, st_list_t *all_elements);

/* ulock the table */
int st_table_unlock(st_table_t *table);

/**
 * clean process table reference,
 * if the process was died, the refcnt which holded by the process can not be released,
 * so you can call the function to cleanup the process refcnt.
 * and the memory will not be leakiness
 */
int st_table_clean_process_ref(st_table_pool_t *pool, pid_t pid);

#endif /* _TABLE_H_INCLUDED_ */
