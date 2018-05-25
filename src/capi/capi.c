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


/** by default, run gc periodically */
#define ST_CAPI_GC_PERIODICALLY    1


/**
 * used to store info used by worker process regularly
 */
static st_capi_process_t *process_state;


st_capi_process_t *
st_capi_get_process_state(void)
{
    return process_state;
}


static inline ssize_t
st_capi_get_size_by_type(void *caddr, st_types_t type)
{
    ssize_t len = 0;
    switch (type) {
        case ST_TYPES_STRING:
            len = strlen(*(char **)caddr);

            break;
        case ST_TYPES_NUMBER:
            len = sizeof(double);

            break;
        case ST_TYPES_BOOLEAN:
            len = sizeof(st_bool);

            break;
        case ST_TYPES_TABLE:
            /** st_tvalue_t.bytes is type of st_table_t **) */
            len = sizeof(st_table_t *);

            break;
        case ST_TYPES_INTEGER:
            len = sizeof(int);

            break;
        case ST_TYPES_U64:
            len = sizeof(uint64_t);

            break;
        default:

            st_assert(0, "unknown tvalue type: %d", type);
    }

    return len;
}

/**
 * caddr is the address of a c value, and is used as the buffer of st_tvalue_t,
 * so its lifecycle must not be shorter than the returned st_tvalue_t.
 */
st_tvalue_t
st_capi_init_tvalue(void *caddr, st_types_t type, ssize_t size)
{
    st_assert(size >= 0);
    st_assert_nonull(caddr);

    if (size == 0) {
        size = st_capi_get_size_by_type(caddr, type);
    }
    else if (type != ST_TYPES_STRING) {
        st_assert(st_capi_get_size_by_type(caddr, type) == size);
    }

    uint8_t *bytes = (uint8_t *)caddr;
    if (type == ST_TYPES_STRING) {
        bytes = *(uint8_t **)caddr;
    }

    return (st_tvalue_t)st_str_wrap_common(bytes, type, size);
}


static int
st_capi_remove_gc_root(st_capi_t *state, st_table_t *table)
{
    st_assert_nonull(state);
    st_assert_nonull(table);

    int ret = st_table_remove_all(table);
    if (ret != ST_OK) {
        derr("failed to remove all from root: %d", ret);

        return ret;
    }

    ret = st_gc_remove_root(&state->table_pool.gc, &table->gc_head);
    if (ret != ST_OK) {
        derr("failed to remove root from gc: %d", ret);

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

    while (ret != ST_ITER_FINISH) {
        ret = st_table_iter_next(table, &iter, &key, &value);

        if (ret == ST_TABLE_MODIFIED) {
            break;
        }

        if (ret == ST_OK) {
            /** ST_ITER_FINISH would stop iterating normally */
            ret = foreach_cb(&key, &value, args);
            if (ret != ST_OK) {
                break;
            }
        }
    }

    return (ret == ST_ITER_FINISH ? ST_OK : ret);
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
st_capi_do_clean_dead_proot(int max_num, int clean_self, int *cleaned)
{
    st_must(max_num >= 0, ST_ARG_INVALID);

    st_capi_t *lib_state      = process_state->lib_state;
    st_capi_process_t *next   = NULL;
    st_capi_process_t *pstate = NULL;

    st_robustlock_lock(&lib_state->lock);

    int num = 0;
    int ret = ST_OK;
    st_list_for_each_entry_safe(pstate, next, &lib_state->proots, node) {
        if (pstate == process_state) {
            pstate = (clean_self ? pstate : NULL);
        }
        else {
            ret = st_robustlock_trylock(&pstate->alive);
            pstate = (ret == ST_OK ? pstate : NULL);
        }

        if (pstate) {
            st_robustlock_unlock(&pstate->alive);
            st_robustlock_destroy(&pstate->alive);
            st_list_remove(&pstate->node);

            ret = st_capi_remove_gc_root(pstate->lib_state, pstate->proot);
            st_assert_ok(ret, "failed to remove gc root: %d", pstate->pid);

            ret = st_slab_obj_free(&lib_state->table_pool.slab_pool, pstate);
            st_assert_ok(ret, "failed to free process state to slab");

            num++;
            if (max_num != 0 && num == max_num) {
                break;
            }
        }
    }

    *cleaned = num;
    st_robustlock_unlock(&lib_state->lock);

    return ST_OK;
}


int
st_capi_clean_dead_proot(int max_num, int *cleaned)
{
    return st_capi_do_clean_dead_proot(max_num, 0, cleaned);
}


int
st_capi_destroy(void)
{
    st_must(process_state != NULL, ST_UNINITED);
    st_must(process_state->lib_state != NULL, ST_UNINITED);

    st_capi_t *lib_state = process_state->lib_state;

    st_assert(lib_state->init_state >= ST_CAPI_INIT_NONE);
    st_assert(lib_state->init_state <= ST_CAPI_INIT_DONE);

    int ret = ST_OK;
    int cleaned = 0;
    st_table_pool_t *table_pool = &lib_state->table_pool;

    while (lib_state != NULL && lib_state->init_state) {
        ret = ST_OK;

        switch (lib_state->init_state) {
            case ST_CAPI_INIT_PROOT:
                /** remove each items in proots from root set of gc */
                st_capi_do_clean_dead_proot(0, 1, &cleaned);
                dd("clean up %d processes", cleaned);

                st_robustlock_destroy(&lib_state->lock);

                break;
            case ST_CAPI_INIT_GROOT:
                ret = st_capi_remove_gc_root(lib_state, lib_state->groot);
                st_assert_ok(ret, "failed to remove groot");

                lib_state->groot = NULL;

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
                                            lib_state->shm_fn,
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

    /** allocate and add groot to gc */
    st_table_t *groot = NULL;
    int ret = st_table_new(&state->table_pool, &groot);
    if (ret != ST_OK) {
        derr("failed to alloc groot: %d", ret);

        return ret;
    }

    /** add groot to gc */
    ret = st_gc_add_root(&state->table_pool.gc, &groot->gc_head);
    if (ret != ST_OK) {
        st_assert(st_table_free(groot) == ST_OK);

        derr("failed to add groot to gc: %d", ret);
        return ret;
    }

    state->groot = groot;
    state->init_state = ST_CAPI_INIT_GROOT;

    st_list_init(&state->proots);
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

    st_capi_process_t *new_pstate = NULL;
    int ret = st_slab_obj_alloc(slab_pool,
                                sizeof(*new_pstate),
                                (void **)&new_pstate);
    if (ret != ST_OK) {
        derr("failed to alloc process state from slab: %d, %d", getpid(), ret);

        return ret;
    }

    new_pstate->pid       = getpid();
    new_pstate->proot     = NULL;
    new_pstate->lib_state = (*pstate)->lib_state;

    st_list_init(&new_pstate->node);

    ret = st_table_new(&lib_state->table_pool, &new_pstate->proot);
    if (ret != ST_OK) {
        derr("failed to new table for pstate: %d, %d", new_pstate->pid, ret);

        goto err_quit;
    }

    ret = st_gc_add_root(&table_pool->gc, &new_pstate->proot->gc_head);
    if (ret != ST_OK) {
        derr("failed to add proot to gc: %d, %d", new_pstate->pid, ret);

        goto err_quit;
    }

    /** alive lock used for process crashing detection */
    ret = st_robustlock_init(&new_pstate->alive);
    if (ret != ST_OK) {
        derr("failed to init pstate alive lock: %d, %d", new_pstate->pid, ret);

        goto err_quit;
    }
    st_robustlock_lock(&new_pstate->alive);

    st_robustlock_lock(&lib_state->lock);
    st_list_insert_last(&lib_state->proots, &new_pstate->node);
    st_robustlock_unlock(&lib_state->lock);

    new_pstate->inited = 1;
    *pstate = new_pstate;

    return ST_OK;

err_quit:
    if (new_pstate->proot != NULL) {
        st_gc_remove_root(&table_pool->gc, &new_pstate->proot->gc_head);

        st_assert_ok(st_table_free(new_pstate->proot),
                     "failed to free process root: %d", new_pstate->pid);
    }

    st_assert_ok(st_slab_obj_free(slab_pool, new_pstate),
                 "failed to free pstate to slab: %d", getpid());

    return ret;
}


/**
 * call again only on failure in parent process.
 */
static int
st_capi_do_init(const char *shm_fn)
{
    st_assert_nonull(shm_fn);

    st_capi_process_t pstate;

    int shm_fd = -1;
    void *base = NULL;

    ssize_t page_size = st_page_size();
    ssize_t meta_size = st_align(sizeof(st_capi_t), page_size);
    ssize_t data_size = st_align(ST_REGION_CNT * ST_REGION_SIZE, page_size);
    ssize_t length    = meta_size + data_size;

    int ret = st_region_shm_create(shm_fn, length, &base, &shm_fd);
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
    state->len        = length;
    state->init_state = ST_CAPI_INIT_SHM;
    memcpy(state->shm_fn, shm_fn, strlen(shm_fn));

    const char *lib_version = st_version_get_fully();
    memcpy(state->version, lib_version, strlen(lib_version));

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
    st_assert_ok(st_capi_destroy(), "critical error, double fault in destroy");

    return ret;
}


int
st_capi_init(const char *shm_fn)
{
    st_assert_nonull(shm_fn);

    if (process_state != NULL) {
        dinfo("module is already inited");

        return ST_OK;
    }

    return st_capi_do_init(shm_fn);
}


int
st_capi_do_add(st_table_t *table, st_tvalue_t key, st_tvalue_t value, int force)
{
    st_assert_nonull(table);
    st_assert(key.type != ST_TYPES_TABLE);

    int ret;
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


int
st_capi_do_remove_key(st_table_t *table, st_tvalue_t key)
{
    st_assert_nonull(table);
    st_assert_nonull(key.bytes);
    st_assert(key.type != ST_TYPES_TABLE);

    int ret = st_table_remove_key(table, key);
    if (ret != ST_OK) {
        derr("failed to remove key from table: %d", ret);

        return ret;
    }

    return ST_OK;
}


static uintptr_t
st_capi_make_proot_table_key(st_tvalue_t tbl_val)
{
    return (uintptr_t)tbl_val.bytes;
}


/** add or remove table reference in proot */
static int
st_capi_handle_table_ref_in_proot(st_tvalue_t tbl_val, int do_add)
{
    st_assert_nonull(tbl_val.bytes);

    uintptr_t key = st_capi_make_proot_table_key(tbl_val);

    if (do_add) {
        st_table_t *table = st_table_get_table_addr_from_value(tbl_val);

        return st_capi_add(process_state->proot, key, table);
    }

    return st_capi_remove_key(process_state->proot, key);
}


int
st_capi_worker_init(void)
{
    /**
     * for now, process_state is the same as in parent.
     */
    st_assert_nonull(process_state);
    st_assert_nonull(process_state->lib_state);
    st_assert(process_state->lib_state->init_state == ST_CAPI_INIT_DONE);

    return st_capi_init_process_state(&process_state);
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
        ret = st_capi_handle_table_ref_in_proot(value, 1);

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
st_capi_get_groot(st_tvalue_t *ret_val)
{
    st_assert_nonull(process_state);

    st_tvalue_t tvalue = st_capi_make_tvalue(process_state->lib_state->groot);

    /**
     * reuse st_capi_copy_out_tvalue logic to st_malloc memory
     * for st_tvalue_t.bytes and handle table reference in proot
     */
    return st_capi_copy_out_tvalue(ret_val, &tvalue);
}


int
st_capi_new(st_tvalue_t *ret_val)
{
    st_table_t *table = NULL;
    int ret = st_table_new(&process_state->lib_state->table_pool, &table);
    if (ret != ST_OK) {
        derr("failed to create table: %d", ret);

        return ret;
    }
    st_tvalue_t tvalue = st_capi_make_tvalue(table);

    /**
     * reuse st_capi_copy_out_tvalue logic to st_malloc memory
     * for st_tvalue_t.bytes and handle table reference in proot
     */
    ret = st_capi_copy_out_tvalue(ret_val, &tvalue);
    if (ret != ST_OK) {
        st_assert_ok(st_table_free(table), "failed to free table");
    }

    return ret;
}


int
st_capi_free(st_tvalue_t *value)
{
    st_assert_nonull(value);
    st_assert_nonull(value->bytes);

    if (st_types_is_table(value->type)) {
        /** remove table ref in proot */
        int ret = st_capi_handle_table_ref_in_proot(*value, 0);

        if (ret != ST_OK) {
            derr("failed to remove table reference: %d", ret);
            return ret;
        }
    }

    st_free(value->bytes);
    *value = (st_tvalue_t)st_str_null;

    return ST_OK;
}


int
st_capi_do_get(st_table_t *table, st_tvalue_t key, st_tvalue_t *ret_val)
{
    st_assert_nonull(table);
    st_assert_nonull(ret_val);
    st_assert_nonull(key.bytes);
    st_assert(key.type != ST_TYPES_TABLE);

    st_robustlock_lock(&table->lock);

    st_tvalue_t value;
    int ret = st_table_get_value(table, key, &value);
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
st_capi_init_iterator(st_tvalue_t *tbl_val,
                      st_capi_iter_t *iter,
                      st_tvalue_t *init_key,
                      int expected_side)
{
    st_assert_nonull(tbl_val);
    st_assert_nonull(iter);

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
    st_assert_nonull(iter);
    st_assert_nonull(ret_key);
    st_assert_nonull(ret_value);

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
    st_assert_nonull(iter);

    return st_capi_free(&iter->table);
}
