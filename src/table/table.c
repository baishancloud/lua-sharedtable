#include "table.h"

int st_table_clear(st_table_t *table);

static int st_table_cmp_element(st_rbtree_node_t *a, st_rbtree_node_t *b) {

    st_table_element_t *ea = st_owner(a, st_table_element_t, rbnode);
    st_table_element_t *eb = st_owner(b, st_table_element_t, rbnode);

    return st_str_cmp(&ea->key, &eb->key);
}

static int st_table_new_element(st_table_t *table, st_str_t key,
                                st_str_t value, st_table_element_t **elem) {

    st_table_pool_t *pool = table->pool;
    st_table_element_t *e = NULL;

    ssize_t size = sizeof(st_table_element_t) + key.len + value.len;

    int ret = st_slab_obj_alloc(&pool->slab_pool, size, (void **)&e);
    if (ret != ST_OK) {
        return ret;
    }

    e->rbnode = (st_rbtree_node_t)st_rbtree_node_empty;

    st_memcpy(e->kv_data, key.bytes, key.len);
    e->key = (st_str_t)st_str_wrap(e->kv_data, key.len);

    st_memcpy(e->kv_data + key.len, value.bytes, value.len);
    e->value = (st_str_t)st_str_wrap(e->kv_data + key.len, value.len);

    *elem = e;

    return ST_OK;
}

static int st_table_release_element(st_table_t *table, st_table_element_t *elem) {

    st_table_pool_t *pool = table->pool;

    return st_slab_obj_free(&pool->slab_pool, elem);
}

static int st_table_add_element(st_table_t *table, st_table_element_t *elem) {

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    st_rbtree_node_t *n = st_rbtree_search_eq(&table->elements, &elem->rbnode);
    if (n != NULL) {
        ret = ST_EXISTED;
        goto quit;
    }

    st_rbtree_insert(&table->elements, &elem->rbnode);

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}

static int st_table_get_element(st_table_t *table, st_str_t key, st_table_element_t **elem) {

    st_table_element_t tmp = {.key = key};

    st_rbtree_node_t *n = st_rbtree_search_eq(&table->elements, &tmp.rbnode);
    if (n == NULL) {
        return ST_NOT_FOUND;
    }

    *elem = st_owner(n, st_table_element_t, rbnode);

    return ST_OK;
}

static int st_table_get_bigger_element(st_table_t *table, st_str_t key, st_table_element_t **elem) {

    st_table_element_t tmp = {.key = key};

    st_rbtree_node_t *n = st_rbtree_search_bigger(&table->elements, &tmp.rbnode);
    if (n == NULL) {
        return ST_NOT_FOUND;
    }

    *elem = st_owner(n, st_table_element_t, rbnode);

    return ST_OK;
}

static int st_table_remove_element(st_table_t *table, st_str_t key, st_table_element_t **removed) {

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_get_element(table, key, removed);
    if (ret != ST_OK) {
        goto quit;
    }

    st_rbtree_delete(&table->elements, &(*removed)->rbnode);

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}

static int st_table_init(st_table_t *table, st_table_pool_t *pool) {

    int ret = st_rbtree_init(&table->elements, st_table_cmp_element);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_init(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    table->gc = (st_table_gc_t) {.refcnt = -1, .state = ST_TABLE_UNTRACKED};
    table->pool = pool;
    table->refcnt = 0;
    table->inited = 1;

    return ST_OK;
}

static int st_table_destroy(st_table_t *table) {

    if (st_atomic_load(&table->refcnt) != 0) {
        return ST_NOT_READY;
    }

    int ret = st_table_clear(table);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_destroy(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    table->pool = NULL;
    table->refcnt = 0;
    table->inited = 0;

    return ST_OK;
}

int st_table_incref(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_table_pool_t *pool = table->pool;

    if (pool->enable_gc) {
        /**
         * lock is used to avoid changing table refcnt
         * when called st_table_get_unreachable_tables function
         */
        int ret = st_robustlock_lock(&pool->lock);
        if (ret != ST_OK) {
            return ret;
        }

        st_atomic_incr(&table->refcnt, 1);

        st_robustlock_unlock_err_abort(&pool->lock);
    } else {
        st_atomic_incr(&table->refcnt, 1);
    }

    return ST_OK;
}

int st_table_decref(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    int ret;
    int64_t refcnt;
    st_table_pool_t *pool = table->pool;

    if (pool->enable_gc) {
        ret = st_robustlock_lock(&pool->lock);
        if (ret != ST_OK) {
            return ret;
        }

        refcnt = st_atomic_decr(&table->refcnt, 1);

        if (refcnt == 0) {
            st_list_remove(&table->gc.lnode);
            pool->table_count--;
        }

        st_robustlock_unlock_err_abort(&pool->lock);

    } else {
        refcnt = st_atomic_decr(&table->refcnt, 1);
    }

    if (refcnt == 0) {
        st_table_pool_t *pool = table->pool;

        ret = st_table_destroy(table);
        if (ret != ST_OK) {
            return ret;
        }

        return st_slab_obj_free(&pool->slab_pool, table);
    }

    return ST_OK;
}

int st_table_new(st_table_pool_t *pool, st_table_t **table) {

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(table != NULL, ST_ARG_INVALID);

    st_table_t *t = NULL;
    st_slab_pool_t *slab_pool = &pool->slab_pool;

    int ret = st_slab_obj_alloc(slab_pool, sizeof(st_table_t), (void **)&t);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_init(t, pool);
    if (ret != ST_OK) {
        st_slab_obj_free(slab_pool, t);
        return ret;
    }

    st_atomic_incr(&t->refcnt, 1);

    if (pool->enable_gc) {

        int need_clear = 0;

        ret = st_robustlock_lock(&pool->lock);
        if (ret != ST_OK) {
            st_slab_obj_free(slab_pool, t);
            return ret;
        }

        t->gc.state = ST_TABLE_REACHABLE;
        st_list_insert_last(&pool->table_list, &t->gc.lnode);

        pool->table_count++;

        if (pool->table_count >= ST_TABLE_GC_COUNT_THRESHOLD &&
                time(NULL) - pool->gc_start_tm >= ST_TABLE_GC_TIME_THRESHOLD) {

            need_clear = 1;
        }

        st_robustlock_unlock_err_abort(&pool->lock);

        if (need_clear) {
            int clear_ret = st_table_clear_circular_ref(pool, 0);
            if (clear_ret != ST_OK) {
                derr("clear_circular_ref error %d\n", clear_ret);
            }
        }
    }

    *table = t;

    return ret;
}

int st_table_release(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    return st_table_decref(table);
}

int st_table_add_value(st_table_t *table, st_str_t key, st_str_t value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value.bytes != NULL && value.len > ST_TABLE_VALUE_TYPE_LEN, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;
    st_table_t *t = NULL;

    int ret = st_table_new_element(table, key, value, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    if (st_table_value_is_table(value)) {
        t = st_table_get_table_addr_from_value(value);

        ret = st_table_incref(t);
        if (ret != ST_OK) {
            st_table_release_element(table, elem);
            return ret;
        }
    }

    ret = st_table_add_element(table, elem);
    if (ret != ST_OK) {
        st_table_release_element(table, elem);

        if (t != NULL) {
            st_table_decref(t);
        }
    }

    return ret;
}

int st_table_remove_value(st_table_t *table, st_str_t key) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);

    st_table_element_t *removed = NULL;

    int ret = st_table_remove_element(table, key, &removed);
    if (ret != ST_OK) {
        return ret;
    }

    if (st_table_value_is_table(removed->value)) {
        st_table_t *t = st_table_get_table_addr_from_value(removed->value);

        ret = st_table_decref(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return st_table_release_element(table, removed);
}

/* lock table before use the function */
int st_table_get_value(st_table_t *table, st_str_t key, st_str_t *value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value != NULL, ST_ARG_INVALID);

    st_table_element_t *elem;

    int ret = st_table_get_element(table, key, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    *value = elem->value;

    return ST_OK;
}

/* lock table before use the function */
int st_table_get_bigger_value(st_table_t *table, st_str_t key, st_str_t *value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value != NULL, ST_ARG_INVALID);

    st_table_element_t *elem;

    int ret = st_table_get_bigger_element(table, key, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    *value = elem->value;

    return ST_OK;
}

int st_table_get_table_withref(st_table_t *table, st_str_t key, st_table_t **sub) {

    st_table_t *t = NULL;
    st_str_t value;

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(sub != NULL, ST_ARG_INVALID);

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_get_value(table, key, &value);
    if (ret != ST_OK) {
        goto quit;
    }

    if (!st_table_value_is_table(value)) {
        ret = ST_ARG_INVALID;
        goto quit;
    }

    t = st_table_get_table_addr_from_value(value);

    ret = st_table_incref(t);
    if (ret != ST_OK) {
        goto quit;
    }

    *sub = t;

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}

int st_table_clear(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_rbtree_node_t *n = NULL;
    st_table_element_t *e = NULL;

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    while (!st_rbtree_is_empty(&table->elements)) {

        n = st_rbtree_left_most(&table->elements);

        st_rbtree_delete(&table->elements, n);

        e = st_owner(n, st_table_element_t, rbnode);

        if (st_table_value_is_table(e->value)) {
            st_table_t *t = st_table_get_table_addr_from_value(e->value);

            ret = st_table_decref(t);
            if (ret != ST_OK) {
                goto quit;
            }
        }

        ret = st_table_release_element(table, e);
        if (ret != ST_OK) {
            goto quit;
        }
    }

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}

int st_table_pool_init(st_table_pool_t *pool, int enable_gc) {

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(enable_gc == 0 || enable_gc == 1, ST_ARG_INVALID);

    pool->enable_gc = enable_gc;

    pool->gc_start_tm = time(NULL);
    pool->table_count = 0;

    pool->table_list = (st_list_t)ST_LIST_INIT(pool->table_list);

    return st_robustlock_init(&pool->lock);
}

int st_table_pool_destroy(st_table_pool_t *pool) {

    st_must(pool != NULL, ST_ARG_INVALID);

    if (!st_list_empty(&pool->table_list)) {
        return ST_NOT_READY;
    }

    int ret = st_robustlock_destroy(&pool->lock);
    if (ret != ST_OK) {
        return ret;
    }

    return ST_OK;
}

/* below functions are used for gc */

static int st_table_traverse(st_table_t *table, st_rbtree_node_t *node,
                             st_table_visit_f visit_func, void *arg) {

    st_rbtree_node_t *sentinel = &table->elements.sentinel;

    if (node == sentinel) {
        return ST_OK;
    }

    st_table_element_t *e = st_owner(node, st_table_element_t, rbnode);

    int ret = visit_func(e, arg);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_traverse(table, node->left, visit_func, arg);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_traverse(table, node->right, visit_func, arg);
    if (ret != ST_OK) {
        return ret;
    }

    return ST_OK;
}

static int st_table_visit_decref_in_gc(st_table_element_t *elem, void *data) {

    if (!st_table_value_is_table(elem->value)) {
        return ST_OK;
    }

    st_table_t *t = st_table_get_table_addr_from_value(elem->value);

    st_assert(t->gc.refcnt > 0);

    /* decrease refcnt, to release from parent table reference. */
    t->gc.refcnt--;

    return ST_OK;
}

static int st_table_visit_reachable_in_gc(st_table_element_t *elem, st_list_t *reachable) {

    if (!st_table_value_is_table(elem->value)) {
        return ST_OK;
    }

    st_table_t *t = st_table_get_table_addr_from_value(elem->value);

    if (t->gc.state == ST_TABLE_UNREACHABLE) {
        /**
         * the table temporary moved into unreachable list,
         * but it can be visited from outside table.
         * so move back to the reachable list.
         */
        t->gc.state = ST_TABLE_REACHABLE;
        t->gc.refcnt = 1;
        st_list_move_tail(&t->gc.lnode, reachable);

    } else if (t->gc.refcnt == 0) {
        /**
         * if refcnt is 0, it will be moved into unreachable list,
         * but the table can be visited from outside table.
         * so set refcnt to 1.
         */
        t->gc.refcnt = 1;
    }

    return ST_OK;
}

static int st_table_get_unreachable_tables(st_table_pool_t *pool, st_list_t *unreachable) {

    st_table_gc_t *tmp = NULL, *next = NULL;
    st_table_t *t = NULL;
    int ret;

    /* get each table refcnt copy, then will calc the copied refcnt to get unreachable tables.*/
    st_list_for_each_entry(tmp, &pool->table_list, lnode) {
        t = st_owner(tmp, st_table_t, gc);

        st_assert(t->refcnt > 0);
        st_assert(t->gc.state == ST_TABLE_REACHABLE);

        t->gc.refcnt = t->refcnt;
    }

    /**
     * release the sub table reference from each table in table_list.
     * if the sub table refcnt is 0, mean no process hold reference.
     * it may be as sub table in some table.
     */
    st_list_for_each_entry(tmp, &pool->table_list, lnode) {
        t = st_owner(tmp, st_table_t, gc);

        ret = st_table_traverse(t, t->elements.root, st_table_visit_decref_in_gc, NULL);
        if (ret != ST_OK) {
            return ret;
        }
    }

    /**
     * after decrease refcnt in upper code,
     * if refcnt is 0, mean no process hold reference.
     * so we temporary assume the table is unreachable.
     * then traverse the table in table_list which refcnt > 0, means other process hold reference.
     * if sub table in the traversed table refcnt is 0 or in unreachable state.
     * then set refcnt to 1 and reachable state, because it can be visited from the traversed table.
     * after traversed the tables, if table in unreachable state, it must be unreachable table.
     * because it cann't be visited from outside table.
     */
    st_list_for_each_entry_safe(tmp, next, &pool->table_list, lnode) {
        t = st_owner(tmp, st_table_t, gc);

        if (t->gc.refcnt > 0) {
            t->gc.state = ST_TABLE_REACHABLE;

            ret = st_table_traverse(t, t->elements.root, (st_table_visit_f)st_table_visit_reachable_in_gc,
                                    &pool->table_list);
            if (ret != ST_OK) {
                return ret;
            }

        } else {
            t->gc.state = ST_TABLE_UNREACHABLE;
            st_list_move_tail(&t->gc.lnode, unreachable);
        }
    }

    return ST_OK;
}

int st_table_clear_circular_ref(st_table_pool_t *pool, int force) {

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(pool->enable_gc, ST_UNSUPPORTED);
    st_must(force == 0 || force == 1, ST_ARG_INVALID);

    st_table_t *t = NULL;
    st_table_gc_t *tmp = NULL;
    st_list_t unreachable = ST_LIST_INIT(unreachable);

    int ret = st_robustlock_lock(&pool->lock);
    if (ret != ST_OK) {
        return ret;
    }

    if (!force && (pool->table_count < ST_TABLE_GC_COUNT_THRESHOLD ||
                   time(NULL) - pool->gc_start_tm < ST_TABLE_GC_TIME_THRESHOLD)) {

        st_robustlock_unlock_err_abort(&pool->lock);
        return ST_NOT_READY;
    }

    pool->gc_start_tm = time(NULL);

    ret = st_table_get_unreachable_tables(pool, &unreachable);

    st_robustlock_unlock_err_abort(&pool->lock);

    if (ret != ST_OK) {
        return ret;
    }

    while (!st_list_empty(&unreachable)) {

        tmp = st_list_first_entry(&unreachable, st_table_gc_t, lnode);
        t = st_owner(tmp, st_table_t, gc);

        /* avoid table has been release in clear, so incref first */
        ret = st_table_incref(t);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_table_clear(t);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_table_decref(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return ST_OK;
}
