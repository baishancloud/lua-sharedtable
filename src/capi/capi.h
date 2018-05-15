#ifndef __ST_CAPI_H_INCLUDE__
#define __ST_CAPI_H_INCLUDE__


#include <limits.h>

#include "inc/types.h"
#include "inc/util.h"
#include "pagepool/pagepool.h"
#include "region/region.h"
#include "robustlock/robustlock.h"
#include "str/str.h"
#include "table/table.h"
#include "version/version.h"


#ifdef MEM_SMALL
#define ST_REGION_CNT  (10U)
#define ST_REGION_SIZE (1024U * 1024U * 10U)
#else
#define ST_REGION_CNT  (64U)
#define ST_REGION_SIZE (1024U * 1024U * 1024U)
#endif


#define st_capi_get_type(cvalue) _Generic((cvalue),    \
    int                       : ST_TYPES_INTEGER,      \
    char *                    : ST_TYPES_STRING,       \
    double                    : ST_TYPES_NUMBER,       \
    uint64_t                  : ST_TYPES_U64,          \
    st_bool                   : ST_TYPES_BOOLEAN,      \
    st_table_t  *             : ST_TYPES_TABLE         \
)


typedef st_str_t                 st_tvalue_t;
typedef struct st_capi_s         st_capi_t;
typedef struct st_capi_iter_s    st_capi_iter_t;
typedef struct st_capi_process_s st_capi_process_t;

/** state variable for iterator */
struct st_capi_iter_s {
    /** use addr of table.bytes to generate unique key in proot */
    st_tvalue_t     table;
    st_table_iter_t iterator;
};

typedef enum st_capi_init_state_e {
    ST_CAPI_INIT_NONE     = 0x01,
    ST_CAPI_INIT_SHM      = 0x02,
    ST_CAPI_INIT_REGION   = 0x03,
    ST_CAPI_INIT_PAGEPOOL = 0x04,
    ST_CAPI_INIT_SLAB     = 0x05,
    ST_CAPI_INIT_TABLE    = 0x06,
    ST_CAPI_INIT_GROOT    = 0x07,
    ST_CAPI_INIT_PROOT    = 0x08,
    ST_CAPI_INIT_DONE     = 0x09,
} st_capi_init_state_t;

/** worker process state */
struct st_capi_process_s {
    int             inited;

    pid_t           pid;
    st_table_t      *root;
    st_capi_t       *lib_state;
    st_list_t       node;
    pthread_mutex_t alive;
};

/** library state */
struct st_capi_s {
    int  shm_fd;
    /** start address of shared memory */
    void *base;
    /** length of the whole shared memory */
    ssize_t len;
    /** version info major.minor.release, like 1.2.3 */
    char version[ST_VERSION_LEN_MAX];
    char shm_fn[NAME_MAX+1];
    /** start address of capi data section */
    void *data;

    st_list_t       p_roots;
    pthread_mutex_t lock;

    st_table_t *g_root;

    st_table_pool_t table_pool;

    st_capi_init_state_t init_state;
};


st_capi_process_t *st_capi_get_process_state(void);

/** module init called by master process */
int st_capi_init(const char *shm_fn);

int st_capi_destroy(void);

int st_capi_worker_init(void);

#define st_capi_value_type_addr(key) \
    (st_tvalue_t)st_str_wrap_common(&(key), st_capi_get_type(key), 0)

#define st_capi_make_tvalue(tvalue, cvalue) \
    st_capi_init_tvalue(&(tvalue), st_capi_value_type_addr(cvalue))

int st_capi_init_tvalue(st_tvalue_t *tvalue, st_tvalue_t src);

/**
 * ret_val.bytes is allocated by st_malloc in st_str_init(),
 * (1) caller should call st_capi_free on ret_val.
 *
 * (2) internally, we use ret_val.bytes to generate unique key for proot.
 *
 * (3) note: since malloc has its cache logic, it may return the same addr
 *           in two separated function call, so the liftcycle of ret_val.bytes
 *           should be no shorter than the table it refers to,
 */
int st_capi_new(st_tvalue_t *ret_val);

int st_capi_free(st_tvalue_t *value);

#define st_capi_set(table, key, value)             \
    st_capi_do_add((table),                        \
                   st_capi_value_type_addr(key),   \
                   st_capi_value_type_addr(value), \
                   1)

#define st_capi_add(table, key, value)             \
    st_capi_do_add((table),                        \
                   st_capi_value_type_addr(key),   \
                   st_capi_value_type_addr(value), \
                   0)

int st_capi_do_add(st_table_t *table,
                   st_tvalue_t k_type_addr,
                   st_tvalue_t v_type_addr,
                   int force);

#define st_capi_get(table, key, tval_ptr) \
    st_capi_do_get((table), st_capi_value_type_addr(key), (tval_ptr))

/**
 * here, we copy tvalue.
 * ret_val.bytes is allocated by st_malloc() in st_str_copy(),
 * so caller should free it by calling st_capi_free().
 */
int st_capi_do_get(st_table_t *table,
                   st_tvalue_t k_type_addr,
                   st_tvalue_t *ret_val);

#define st_capi_remove_key(table, key) \
    st_capi_do_remove_key((table), st_capi_value_type_addr(key))

int st_capi_do_remove_key(st_table_t *table, st_tvalue_t k_type_addr);

typedef int (*st_capi_foreach_cb_t)(const st_tvalue_t *key,
                                    st_tvalue_t *value,
                                    void *args);

int st_capi_foreach(st_table_t *table,
                    st_tvalue_t *init_key,
                    int expected_side,
                    st_capi_foreach_cb_t foreach_cb,
                    void *args);

int st_capi_recycle_roots(int max_num, int *recycle_num);

int st_capi_init_iterator(st_tvalue_t *tbl_val,
                          st_capi_iter_t *iter,
                          st_tvalue_t *init_key,
                          int expected_side);

int st_capi_next(st_capi_iter_t *iter,
                 st_tvalue_t *ret_key,
                 st_tvalue_t *ret_value);

int st_capi_free_iterator(st_capi_iter_t *iter);

#endif
