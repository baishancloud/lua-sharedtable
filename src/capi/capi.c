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
static st_capi_process_t *process_state;


st_capi_process_t *
st_capi_get_process_state(void)
{
    return process_state;
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

    ret = st_gc_remove_root(&state->table_pool.gc, &table->gc_head);
    if (ret != ST_OK) {
        derr("failed to remove proot from gc: %d", ret);

        return ret;
    }

    return st_table_free(table);
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


/** called only by master process */
static int
st_capi_do_recycle_roots(int max_num, int recycle_self, int *recycle_num)
{
    st_must(max_num >= 0, ST_ARG_INVALID);

    st_capi_t *lib_state      = process_state->lib_state;
    st_capi_process_t *next   = NULL;
    st_capi_process_t *pstate = NULL;

    st_robustlock_lock(&lib_state->lock);

    int num = 0;
    int ret = ST_OK;
    st_list_for_each_entry_safe(pstate, next, &lib_state->p_roots, node) {
        if (pstate->pid == process_state->pid) {
            pstate = (recycle_self ? pstate : NULL);
        }
        else {
            ret = st_robustlock_trylock(&pstate->alive);
            pstate = (ret == ST_OK ? pstate : NULL);
        }

        if (pstate) {
            st_robustlock_unlock(&pstate->alive);
            st_robustlock_destroy(&pstate->alive);
            st_list_remove(&pstate->node);

            ret = st_capi_remove_gc_root(pstate->lib_state, pstate->root);
            st_assert_ok(ret, "failed to remove gc root: %d", pstate->pid);

            ret = st_slab_obj_free(&lib_state->table_pool.slab_pool, pstate);
            st_assert_ok(ret, "failed to free process state to slab");

            num++;
            if (max_num != 0 && num == max_num) {
                break;
            }
        }
    }

    *recycle_num = num;
    st_robustlock_unlock(&lib_state->lock);

    return ST_OK;
}


int
st_capi_recycle_roots(int max_num, int *recycle_num)
{
    return st_capi_do_recycle_roots(max_num, 0, recycle_num);
}


int
st_capi_destroy(void)
{
    // TODO: lsl, remove it
    //st_capi_validate_lib_state(process_state.lib_state);

    int ret = ST_OK;
    int recycled = 0;
    st_capi_t *lib_state = process_state->lib_state;
    st_table_pool_t *table_pool = &lib_state->table_pool;

    while (lib_state != NULL && lib_state->init_state) {
        ret = ST_OK;

        switch (lib_state->init_state) {
            case ST_CAPI_INIT_PROOT:
                /** remove each items in p_roots from root set of gc */
                st_capi_do_recycle_roots(0, 1, &recycled);
                dd("recycle %d processes", recycled);

                st_robustlock_destroy(&lib_state->lock);

                break;
            case ST_CAPI_INIT_GROOT:
                ret = st_capi_remove_gc_root(lib_state, lib_state->g_root);
                st_assert_ok(ret, "failed to remove g_root");

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
                    lib_state     = NULL;
                    process_state = NULL;
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

    state->g_root = root;
    state->init_state = ST_CAPI_INIT_GROOT;

    st_list_init(&state->p_roots);
    ret = st_robustlock_init(&state->lock);
    if (ret != ST_OK) {
        derr("failed to init lib state lock: %d", ret);

        return ret;
    }

    state->init_state = ST_CAPI_INIT_PROOT;

    return ST_OK;
}


static int
st_capi_init_process_state(st_capi_process_t **pstate)
{
    st_capi_t *lib_state        = (*pstate)->lib_state;
    st_table_pool_t *table_pool = &lib_state->table_pool;
    st_slab_pool_t *slab_pool   = &lib_state->table_pool.slab_pool;

    st_capi_process_t *new = NULL;
    int ret = st_slab_obj_alloc(slab_pool, sizeof(*new), (void **)&new);
    if (ret != ST_OK) {
        derr("failed to alloc process state from slab: %d, %d", getpid(), ret);

        return ret;
    }

    new->pid       = getpid();
    new->root      = NULL;
    new->lib_state = (*pstate)->lib_state;

    st_list_init(&new->node);

    ret = st_table_new(&lib_state->table_pool, &new->root);
    if (ret != ST_OK) {
        derr("failed to new table for process state: %d, %d", new->pid, ret);

        goto err_quit;
    }

    ret = st_gc_add_root(&table_pool->gc, &new->root->gc_head);
    if (ret != ST_OK) {
        derr("failed to add proot to gc: %d, %d", new->pid, ret);

        goto err_quit;
    }

    /** alive lock used for process crashing detection */
    ret = st_robustlock_init(&new->alive);
    if (ret != ST_OK) {
        derr("failed to init process state alive lock: %d, %d", new->pid, ret);

        goto err_quit;
    }
    st_robustlock_lock(&new->alive);

    st_robustlock_lock(&lib_state->lock);
    st_list_insert_last(&lib_state->p_roots, &new->node);
    st_robustlock_unlock(&lib_state->lock);

    new->inited = 1;
    *pstate = new;

    return ST_OK;

err_quit:
    if (new->root != NULL) {
        st_gc_remove_root(&table_pool->gc, &new->root->gc_head);

        st_assert_ok(st_table_free(new->root),
                     "failed to free process root: %d", new->pid);
    }

    st_assert_ok(st_slab_obj_free(slab_pool, new),
                 "failed to free process state to slab: %d", getpid());

    return ret;
}


/**
 * call again only on failure in parent process.
 */
int
st_capi_init(void)
{
    st_capi_process_t pstate;

    int shm_fd = -1;
    void *base = NULL;

    ssize_t page_size = st_page_size();
    ssize_t meta_size = st_align(sizeof(st_capi_t), page_size);
    ssize_t data_size = st_align(ST_REGION_CNT * ST_REGION_SIZE, page_size);

    int ret = st_region_shm_create(meta_size + data_size, &base, &shm_fd);
    if (ret != ST_OK) {
        derr("failed to create shm: %d", ret);

        return ret;
    }

    process_state    = &pstate;
    pstate.lib_state = (st_capi_t *)base;
    st_capi_t *state = pstate.lib_state;

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

    ret = st_capi_init_process_state(&process_state);
    if (ret != ST_OK) {
        derr("failed to init process state: %d", ret);

        goto err_quit;
    }
    process_state->lib_state->init_state = ST_CAPI_INIT_DONE;

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
        return st_capi_add(process_state->root, key, table);
    }

    return st_capi_remove_key(process_state->root, key);
}


int
st_capi_worker_init(void)
{
    /**
     * for now, process_state is the same as in parent.
     */
    st_capi_validate_pstate(process_state);

    return st_capi_init_process_state(&process_state);
}


int
st_capi_new(st_tvalue_t *ret_val)
{
    st_must(ret_val != NULL, ST_ARG_INVALID);

    st_table_t *table = NULL;
    st_tvalue_t tvalue = st_str_wrap_common(NULL, ST_TYPES_TABLE, 0);
    int ret = st_str_init(&tvalue, sizeof(st_table_t *));
    if (ret != ST_OK) {
        derr("failed to init st_tvalue_t: %d", ret);

        return ret;
    }

    ret = st_table_new(&process_state->lib_state->table_pool, &table);
    if (ret != ST_OK) {
        derr("failed to create table: %d", ret);

        goto err_quit;
    }

    /** use addr as key to add table in proot. */
    ret = st_capi_handle_table_ref((void *)tvalue.bytes, table);
    if (ret != ST_OK) {
        derr("failed to set table: %d", ret);

        goto err_quit;
    }

    *ret_val = tvalue;
    *((st_table_t **)ret_val->bytes) = table;

    return ST_OK;

err_quit:
    if (table != NULL) {
        st_assert_ok(st_table_free(table), "failed to free table");
    }
    st_free(tvalue.bytes);

    return ret;
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
