/**
 * capi.c
 *
 * capi module for c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "capi.h"


#define st_capi_validate_lib_state(lib_state) do {                            \
    st_must((lib_state) != NULL, ST_ARG_INVALID);                             \
    st_must((lib_state)->init_state >= ST_CAPI_INIT_NONE, ST_ARG_INVALID);    \
    st_must((lib_state)->init_state <= ST_CAPI_INIT_DONE, ST_ARG_INVALID);    \
} while (0)

#define st_capi_validate_pstate(pstate) do {                                   \
    st_typeof(pstate) _pstate = pstate;                                        \
                                                                               \
    st_must(_pstate != NULL, ST_UNINITED);                                     \
    st_must(_pstate->inited == 1, ST_UNINITED);                                \
    st_must(_pstate->pid != 0, ST_UNINITED);                                   \
    st_must(_pstate->root != NULL, ST_UNINITED);                               \
                                                                               \
    st_must(_pstate->lib_state != NULL, ST_UNINITED);                          \
    st_must(_pstate->lib_state->init_state == ST_CAPI_INIT_DONE, ST_UNINITED); \
} while (0)


/** by default, run gc periodically */
#define ST_CAPI_GC_PERIODICALLY    1

#define st_capi_process_empty {    \
    .pid       = 0,                \
    .root      = NULL,             \
    .inited    = 0,                \
    .lib_state = NULL,             \
}


/**
 * used to store info used by worker process regularly
 */
static st_capi_process_t process_state = st_capi_process_empty;


st_capi_process_t *
st_capi_get_process_state(void)
{
    return &process_state;
}


/**
 * the address where src.bytes points to is used as the buffer for st_tvalue_t,
 * so its lifecycle must not be shorter than the returned st_tvalue_t.
 *
 * src is used to hold the pointer to value and value type only.
 */
int
st_capi_init_tvalue(st_tvalue_t *tvalue, st_tvalue_t src)
{
    st_must(tvalue != NULL, ST_ARG_INVALID);
    st_must(src.type != ST_TYPES_UNKNOWN, ST_UNSUPPORTED);
    st_must(src.bytes != NULL, ST_ARG_INVALID);

    int64_t len = 0;

    switch (src.type) {
        case ST_TYPES_STRING:
            src.bytes = (uint8_t *)(*((char **)src.bytes));
            len = strlen((char *)src.bytes) + 1;

            break;
        case ST_TYPES_NUMBER:
            len = sizeof(double);

            break;
        case ST_TYPES_BOOLEAN:
            len = sizeof(st_bool);

            break;
        case ST_TYPES_TABLE:
            /** src.bytes is type of st_table_t **) */
            len = sizeof(st_table_t *);

            break;
        case ST_TYPES_INTEGER:
            len = sizeof(int);

            break;
        case ST_TYPES_U64:
            len = sizeof(uint64_t);

            break;
        default:

            return ST_ARG_INVALID;
    }

    *tvalue = (st_tvalue_t)st_str_wrap_common(src.bytes, src.type, len);

    return ST_OK;
}


static int
st_capi_remove_gc_root(st_capi_t *state, st_table_t *table)
{
    st_must(state != NULL, ST_ARG_INVALID);
    st_must(table != NULL, ST_ARG_INVALID);

    int ret = st_table_remove_all(table);
    if (ret != ST_OK) {
        derr("failed to remove all from root: %d", ret);

        return ret;
    }

    return st_gc_remove_root(&state->table_pool.gc, &table->gc_head);
}


static int
st_capi_foreach_nolock(st_table_t *table,
                       st_tvalue_t *init_key,
                       int expected_side,
                       st_capi_foreach_cb_t foreach_cb,
                       void *args)
{
    st_tvalue_t     key;
    st_tvalue_t     value;
    st_table_iter_t iter;

    int ret = st_table_iter_init(table, &iter, init_key, expected_side);
    if (ret != ST_OK) {
        derr("failed to init iter: %d", ret);

        return ret;
    }

    while (ret == ST_OK) {
        ret = st_table_iter_next(table, &iter, &key, &value);

        if (ret == ST_TABLE_MODIFIED) {
            break;
        }

        if (ret == ST_OK) {
            /** ST_ITER_STOP would stop iterating normally */
            ret = foreach_cb(&key, &value, args);
        }
    }

    return (ret == ST_NOT_FOUND ? ST_OK : ret);
}


int
st_capi_foreach(st_table_t *table,
                st_tvalue_t *init_key,
                int expected_side,
                st_capi_foreach_cb_t foreach_cb,
                void *args)
{
    st_robustlock_lock(&table->lock);

    int ret = st_capi_foreach_nolock(table,
                                     init_key,
                                     expected_side,
                                     foreach_cb,
                                     args);

    st_robustlock_unlock(&table->lock);

    return ret;
}


static int
st_capi_remove_gc_root_cb(const st_tvalue_t *key,
                          st_tvalue_t *value,
                          void *roots)
{
    st_assert(ST_TYPES_INTEGER == key->type);

    st_table_t *proot = st_table_get_table_addr_from_value(*value);
    pid_t pid = *((pid_t *)key->bytes);

    errno = 0;
    int ret = kill(pid, 0);
    if (ret != -1 || errno != ESRCH) {
        return ST_OK;
    }

    ret = st_capi_remove_gc_root(process_state.lib_state, proot);
    if (ret != ST_OK) {
        derr("failed to remove gc root: %d", ret);

        return ret;
    }

    pid_t *root_array = (pid_t *)roots;
    int index         = root_array[0]--;
    root_array[index] = pid;

    return (root_array[0] == 0 ? ST_ITER_STOP : ST_OK);
}


int
st_capi_recycle_roots(int max_num, int *recycle_num)
{
    st_must(max_num >= 0, ST_ARG_INVALID);

    st_capi_t *lib_state = process_state.lib_state;

    if (max_num == 0) {
        max_num = lib_state->p_roots->element_cnt;
    }

    pid_t *roots = (pid_t *)calloc(max_num + 1, sizeof(*roots));
    if (roots == NULL) {
        return ST_OUT_OF_MEMORY;
    }
    roots[0] = max_num;

    int ret = st_capi_foreach(lib_state->p_roots,
                              NULL,
                              0,
                              st_capi_remove_gc_root_cb,
                              (void *)roots);

    if (ret == ST_ITER_STOP) {
        ret = ST_OK;
    }

    if (ret != ST_OK) {
        derr("failed to recycle roots: %d", ret);
        goto quit;
    }

    *recycle_num = 0;
    for (int cnt = 1; cnt <= max_num; cnt++) {
        pid_t pid = roots[cnt];
        if (pid == 0) {
            continue;
        }

        int ok = st_capi_remove_key(lib_state->p_roots, pid);
        if (ok != ST_OK) {
            ret = (ret == ST_OK ? ok : ret);
            derr("failed to recycle root: %d, %d", pid, ok);
        }

        (*recycle_num)++;
    }

quit:
    st_free(roots);

    return ret;
}


int
st_capi_destroy(void)
{
    st_capi_validate_lib_state(process_state.lib_state);

    int ret = ST_OK;
    int recycled = 0;
    st_capi_t *lib_state = process_state.lib_state;
    st_table_pool_t *table_pool = &lib_state->table_pool;

    while (lib_state != NULL && lib_state->init_state) {
        ret = ST_OK;

        switch (lib_state->init_state) {
            case ST_CAPI_INIT_PROOT:
                /** remove each items in p_roots from root set of gc */
                ret = st_capi_recycle_roots(0, &recycled);
                if (ret != ST_OK) {
                    break;
                }

                dd("recycle %d processes", recycle_num);
                /** release p_roots */
                ret = st_table_free(lib_state->p_roots);
                lib_state->p_roots = NULL;

                break;
            case ST_CAPI_INIT_GROOT:
                ret = st_capi_remove_gc_root(lib_state, lib_state->g_root);
                if (ret != ST_OK) {
                    break;
                }

                ret = st_table_free(lib_state->g_root);
                lib_state->g_root = NULL;

                break;
            case ST_CAPI_INIT_TABLE:
                ret = st_table_pool_destroy(table_pool);

                break;
            case ST_CAPI_INIT_SLAB:
                ret = st_slab_pool_destroy(&table_pool->slab_pool);

                break;
            case ST_CAPI_INIT_PAGEPOOL:
                ret = st_pagepool_destroy(&table_pool->slab_pool.page_pool);

                break;
            case ST_CAPI_INIT_REGION:
                ret = st_region_destroy(&table_pool->slab_pool.page_pool.region_cb);

                break;
            case ST_CAPI_INIT_SHM:
                ret = st_region_shm_destroy(lib_state->shm_fd,
                                            lib_state->base,
                                            lib_state->len);
                if (ret == ST_OK) {
                    lib_state = NULL;
                    process_state = (st_capi_process_t)st_capi_process_empty;
                }

                break;
            default:

                break;
        }

        if (ret != ST_OK) {
            derr("failed to destroy state at phase: %d, %d",
                 lib_state->init_state,
                 ret);
            break;
        }

        if (lib_state != NULL) {
            lib_state->init_state--;
        }
    }

    return ret;
}


static int
st_capi_master_init_roots(st_capi_t *state)
{
    st_must(state != NULL, ST_ARG_INVALID);

    /** allocate and add g_root to gc */
    st_table_t *root = NULL;
    int ret = st_table_new(&state->table_pool, &root);
    if (ret != ST_OK) {
        derr("failed to alloc g_root: %d", ret);

        return ret;
    }

    /** add g_root to gc */
    ret = st_gc_add_root(&state->table_pool.gc, &root->gc_head);
    if (ret != ST_OK) {
        st_assert(st_table_free(root) == ST_OK);

        derr("failed to add g_root to gc: %d", ret);
        return ret;
    }

    state->g_root     = root;
    state->init_state = ST_CAPI_INIT_GROOT;

    ret = st_table_new(&state->table_pool, &root);
    if (ret != ST_OK) {
        derr("failed to alloc p_roots: %d", ret);
        return ret;
    }

    state->p_roots    = root;
    state->init_state = ST_CAPI_INIT_PROOT;

    return ST_OK;
}


/**
 * call again only on failure in parent process.
 */
int
st_capi_init(void)
{
    st_must(process_state.lib_state == NULL, ST_INITTWICE);

    int shm_fd = -1;
    void *base = NULL;

    ssize_t page_size  = st_page_size();
    ssize_t meta_size = st_align(sizeof(st_capi_t), page_size);
    ssize_t data_size = st_align(ST_REGION_CNT * ST_REGION_SIZE, page_size);

    int ret = st_region_shm_create(meta_size + data_size, &base, &shm_fd);
    if (ret != ST_OK) {
        derr("failed to create shm: %d", ret);

        return ret;
    }

    process_state.lib_state = (st_capi_t *)base;
    st_capi_t *state = process_state.lib_state;

    memset(state, 0, sizeof(*state));

    void *data        = (void *)((uintptr_t)base + meta_size);
    state->base       = base;
    state->data       = data;
    state->shm_fd     = shm_fd;
    state->len        = meta_size + data_size;
    state->init_state = ST_CAPI_INIT_SHM;

    ret = st_region_init(&state->table_pool.slab_pool.page_pool.region_cb,
                         data,
                         ST_REGION_SIZE / page_size,
                         ST_REGION_CNT,
                         1);
    if (ret != ST_OK) {
        derr("failed to init region: %d", ret);

        goto err_quit;
    }
    state->init_state = ST_CAPI_INIT_REGION;

    ret = st_pagepool_init(&state->table_pool.slab_pool.page_pool, page_size);
    if (ret != ST_OK) {
        derr("failed to init pagepool: %d", ret);

        goto err_quit;
    }
    state->init_state = ST_CAPI_INIT_PAGEPOOL;

    ret = st_slab_pool_init(&state->table_pool.slab_pool);
    if (ret != ST_OK) {
        derr("failed to init slab: %d", ret);

        goto err_quit;
    }
    state->init_state = ST_CAPI_INIT_SLAB;

    /** by default, run gc periodically */
    ret = st_table_pool_init(&state->table_pool, ST_CAPI_GC_PERIODICALLY);
    if (ret != ST_OK) {
        derr("failed to init table pool: %d", ret);

        goto err_quit;
    }
    state->init_state = ST_CAPI_INIT_TABLE;

    ret = st_capi_master_init_roots(state);
    if (ret != ST_OK) {
        derr("failed to init root set: %d", ret);

        goto err_quit;
    }
    state->init_state = ST_CAPI_INIT_DONE;

    process_state.pid       = getpid();
    process_state.root      = state->p_roots;
    process_state.lib_state = state;
    process_state.inited    = 1;

    return ST_OK;

err_quit:
    st_assert(st_capi_destroy() == ST_OK);

    return ret;
}


int
st_capi_do_add(st_table_t *table,
               st_tvalue_t kinfo,
               st_tvalue_t vinfo,
               int force)
{
    st_must(table != NULL, ST_ARG_INVALID);
    st_must(kinfo.type != ST_TYPES_TABLE, ST_ARG_INVALID);

    st_tvalue_t key;
    st_tvalue_t value;

    int ret = st_capi_init_tvalue(&key, kinfo);
    if (ret != ST_OK) {
        derr("failed to init key for set: %d", ret);

        return ret;
    }

    ret = st_capi_init_tvalue(&value, vinfo);
    if (ret != ST_OK) {
        derr("failed to init value for set %d", ret);

        return ret;
    }

    if (force) {
        ret = st_table_set_key_value(table, key, value);
    }
    else {
        ret = st_table_add_key_value(table, key, value);
    }

    if (ret != ST_OK) {
        dd("failed to set key value for set: %d", ret);

        return ret;
    }

    return ST_OK;
}


static int
st_capi_handle_table_ref(void *addr_as_key, st_table_t *table)
{
    st_must(addr_as_key != NULL, ST_ARG_INVALID);

    uintptr_t key = (uintptr_t)addr_as_key;
    if (table != NULL) {
        return st_capi_add(process_state.root, key, table);
    }

    return st_capi_remove_key(process_state.root, key);
}


static int
st_capi_do_new(st_table_t **ret_tbl, void *addr_as_key)
{
    st_capi_validate_pstate(&process_state);
    st_must(ret_tbl != NULL, ST_ARG_INVALID);

    st_table_t *table = NULL;

    int ret = st_table_new(&process_state.lib_state->table_pool, &table);
    if (ret != ST_OK) {
        derr("failed to create table: %d", ret);

        return ret;
    }

    /**
     * use pid as key to add proot in lib_state.
     * use addr as key to add table in proot.
     */
    if (addr_as_key != NULL) {
        ret = st_capi_handle_table_ref(addr_as_key, table);
    }
    else {
        ret = st_capi_set(process_state.root, process_state.pid, table);
    }

    if (ret != ST_OK) {
        derr("failed to set table: %d", ret);

        st_table_free(table);
        return ret;
    }

    *ret_tbl = table;

    return ST_OK;
}


int
st_capi_worker_init(void)
{
    /**
     * for now, process_state is the same as in parent.
     */
    st_capi_validate_pstate(&process_state);

    int ret = ST_OK;

    /** allocate p_root, add it to p_roots and root set of gc */
    st_tvalue_t key;
    st_tvalue_t val;

    st_table_t *root  = NULL;
    process_state.pid = getpid();

    st_capi_make_tvalue(key, process_state.pid);

    ret = st_table_get_value(process_state.lib_state->p_roots, key, &val);
    if (ret == ST_OK) {
        root = st_table_get_table_addr_from_value(val);

    }
    else {
        if (ret != ST_NOT_FOUND) {
            derr("failed to get proot: %d, pid: %d", ret, process_state.pid);

            return ret;
        }

        /**
         * notice:
         *   here we add root table of a worker process into proot set which
         *   is logically owned by master process.
         *
         *   for now, process_state is the same as in master process,
         *   so process_state->root refers to proot set.
         */
        ret = st_capi_do_new(&root, NULL);
        if (ret != ST_OK) {
            derr("failed to new process root table: %d, pid: %d",
                 ret,
                 process_state.pid);

            return ret;
        }
    }

    ret = st_gc_add_root(&process_state.lib_state->table_pool.gc,
                         &root->gc_head);
    if (ret != ST_OK && ret != ST_EXISTED) {
        derr("failed to add root to gc: %d", ret);

        return ret;
    }

    /** set process state */
    process_state.root   = root;
    process_state.inited = 1;

    return ST_OK;
}

int
st_capi_new(st_tvalue_t *ret_val)
{
    st_must(ret_val != NULL, ST_ARG_INVALID);

    st_tvalue_t tvalue = st_str_wrap_common(NULL, ST_TYPES_TABLE, 0);
    int ret = st_str_init(&tvalue, sizeof(st_table_t *));
    if (ret != ST_OK) {
        derr("failed to init st_tvalue_t: %d", ret);

        return ret;
    }

    st_table_t *table = NULL;
    ret = st_capi_do_new(&table, tvalue.bytes);
    if (ret != ST_OK) {
        derr("failed to do_new table: %d", ret);

        st_free(tvalue.bytes);
        return ret;
    }

    *ret_val = tvalue;
    *((st_table_t **)ret_val->bytes) = table;

    return ST_OK;
}


int
st_capi_free(st_tvalue_t *value)
{
    st_must(value != NULL, ST_ARG_INVALID);
    st_must(value->bytes != NULL, ST_ARG_INVALID);

    if (st_types_is_table(value->type)) {
        int ret = st_capi_handle_table_ref(value->bytes, NULL);

        if (ret != ST_OK) {
            derr("failed to remove table reference: %d", ret);
            return ret;
        }
    }

    st_free(value->bytes);
    *value = (st_tvalue_t)st_str_null;

    return ST_OK;
}


static int
st_capi_copy_out_tvalue(st_tvalue_t *dst, st_tvalue_t *src)
{
    st_must(dst != NULL, ST_ARG_INVALID);
    st_must(src != NULL, ST_ARG_INVALID);

    st_tvalue_t value = st_str_null;

    int ret = st_str_copy(&value, src);
    if (ret != ST_OK) {
        derr("failed to copy tvalue: %d", ret);

        goto err_quit;
    }

    if (st_types_is_table(value.type)) {
        ret = st_capi_handle_table_ref(value.bytes,
                                       *((st_table_t **)value.bytes));

        if (ret != ST_OK) {
            derr("failed to add table ref key in proot: %d", ret);

            goto err_quit;
        }
    }

    *dst = value;

    return ST_OK;

err_quit:
    if (value.bytes != NULL) {
        st_free(value.bytes);
    }

    return ret;
}


int
st_capi_do_get(st_table_t *table, st_tvalue_t kinfo, st_tvalue_t *ret_val)
{
    st_must(table != NULL, ST_ARG_INVALID);
    st_must(ret_val != NULL, ST_ARG_INVALID);
    st_must(kinfo.bytes != NULL, ST_ARG_INVALID);
    st_must(kinfo.type != ST_TYPES_TABLE, ST_ARG_INVALID);

    st_robustlock_lock(&table->lock);

    st_tvalue_t key;
    int ret = st_capi_init_tvalue(&key, kinfo);
    if (ret != ST_OK) {
        derr("failed to init tvalue of key: %d", ret);

        goto quit;
    }

    st_tvalue_t value;
    ret = st_table_get_value(table, key, &value);
    if (ret != ST_OK) {
        dd("failed to get table value: %d", ret);

        goto quit;
    }

    ret = st_capi_copy_out_tvalue(ret_val, &value);

quit:
    st_robustlock_unlock(&table->lock);

    return ret;
}


int
st_capi_do_remove_key(st_table_t *table, st_tvalue_t kinfo)
{
    st_must(table != NULL, ST_ARG_INVALID);
    st_must(kinfo.bytes != NULL, ST_ARG_INVALID);
    st_must(kinfo.type != ST_TYPES_TABLE, ST_ARG_INVALID);

    st_tvalue_t key;
    int ret = st_capi_init_tvalue(&key, kinfo);
    if (ret != ST_OK) {
        derr("failed to init tvalue for key: %d", ret);

        return ret;
    }

    ret = st_table_remove_key(table, key);
    if (ret != ST_OK) {
        derr("failed to remove key from table: %d", ret);

        return ret;
    }

    return ST_OK;
}


int
st_capi_init_iterator(st_tvalue_t *tbl_val,
                      st_capi_iter_t *iter,
                      st_tvalue_t *init_key,
                      int expected_side)
{
    st_must(tbl_val != NULL, ST_ARG_INVALID);
    st_must(iter != NULL, ST_ARG_INVALID);

    st_table_t *table = st_table_get_table_addr_from_value(*tbl_val);

    st_robustlock_lock(&table->lock);

    int ret = st_table_iter_init(table,
                                 &iter->iterator,
                                 init_key,
                                 expected_side);
    if (ret != ST_OK) {
        goto quit;
    }

    ret = st_capi_copy_out_tvalue(&iter->table, tbl_val);

quit:
    st_robustlock_unlock(&table->lock);

    return ret;
}


int
st_capi_next(st_capi_iter_t *iter, st_tvalue_t *ret_key, st_tvalue_t *ret_value)
{
    st_must(iter != NULL, ST_ARG_INVALID);
    st_must(ret_key != NULL, ST_ARG_INVALID);
    st_must(ret_value != NULL, ST_ARG_INVALID);

    st_table_t *table = st_table_get_table_addr_from_value(iter->table);

    st_robustlock_lock(&table->lock);

    st_tvalue_t key;
    st_tvalue_t value;

    int ret = st_table_iter_next(table, &iter->iterator, &key, &value);
    if (ret == ST_OK) {
        ret = st_capi_copy_out_tvalue(ret_key, &key);
        if (ret != ST_OK) {
            goto quit;
        }

        ret = st_capi_copy_out_tvalue(ret_value, &value);
        if (ret != ST_OK) {
            st_assert(st_capi_free(ret_key) == ST_OK);
        }
    }

quit:
    st_robustlock_unlock(&table->lock);

    return ret;
}


int
st_capi_free_iterator(st_capi_iter_t *iter)
{
    st_must(iter != NULL, ST_ARG_INVALID);

    return st_capi_free(&iter->table);
}
