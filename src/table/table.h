#ifndef _TABLE_H_INCLUDED_
#define _TABLE_H_INCLUDED_

#include <stdint.h>
#include <string.h>

#include "inc/inc.h"

#include "atomic/atomic.h"
#include "list/list.h"
#include "rbtree/rbtree.h"
#include "robustlock/robustlock.h"
#include "slab/slab.h"
#include "str/str.h"

typedef struct st_table_element_s st_table_element_t;
typedef struct st_table_s st_table_t;

struct st_table_element_s {
    /* used for table rbtree */
    st_rbtree_node_t rbnode;

    /* used for putting element into a list when iterate table */
    st_list_t lnode;

    /* key used to identity value, key is no type, user should interpret it */
    st_str_t key;

    /**
     * there are two type value:
     * 1. user interpret type, user define the value format, maybe put type and value into it,
     *    and specify it as int, double, string etc.
     * 2. table reference type, table address is stored in it.
     */
    st_str_t value;

    /* identity whether value is table reference */
    int is_table_ref;

    /* space to store key and value */
    uint8_t kv_data[0];
};

struct st_table_s {
    st_slab_pool_t *slab_pool;

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

/* new a table, and it's refcnt is set to 1 */
int st_table_new(st_slab_pool_t *slab_pool, st_table_t **new);

/* the function decrease table refcnt, if refcnt is 0, table space will be released */
int st_table_release(st_table_t *table);

/**
 * if a process want to use the table,
 * first it should use the function to get table reference.
 * then you will not afraid other process release the table.
 */

int st_table_get_reference(st_table_t *table);

/**
 * if you have got the table reference,
 * you must call the function to return the table reference.
 * if you don't , table will hold refcnt, and the table space will never be released.
 */
int st_table_return_reference(st_table_t *table);

/**
 * you can insert user value or other table reference into table.
 * if you insert table, the table refcnt will be increased 1.
 */
int st_table_insert_value(st_table_t *table, st_str_t key, st_str_t value, int is_table_ref);

/**
 * remove value from table.
 * if value is table reference, the table refcnt will be decreased 1.
 */
int st_table_remove_value(st_table_t *table, st_str_t key);

/**
 * lock table. if you call st_table_get_value or st_table_get_all_sorted_elements function
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
int st_table_get_all_sorted_elements(st_table_t *table, st_list_t *all);

/* ulock the table */
int st_table_unlock(st_table_t *table);

/**
 * get sub table from the table, and the sub table refcnt will be increased 1.
 * the function is locked.
 */
int st_table_get_table_with_reference(st_table_t *table, st_str_t key, st_table_t **sub);

#endif /* _TABLE_H_INCLUDED_ */
