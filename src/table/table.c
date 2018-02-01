#include "table.h"

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
    e->key = (st_str_t)st_str_wrap_common(e->kv_data, key.type, key.len);

    st_memcpy(e->kv_data + key.len, value.bytes, value.len);
    e->value = (st_str_t)st_str_wrap_common(e->kv_data + key.len, value.type, value.len);

    *elem = e;

    return ret;
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

    ret = st_rbtree_insert(&table->elements, &elem->rbnode, 0);
    if (ret != ST_OK) {
        goto quit;
    }

    st_atomic_incr(&table->element_cnt, 1);

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
    st_atomic_decr(&table->element_cnt, 1);

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

    st_gc_head_init(&pool->gc, &table->gc_head);

    table->pool = pool;
    table->element_cnt = 0;
    table->inited = 1;

    return ret;
}

static int st_table_destroy(st_table_t *table) {

    if (table->element_cnt != 0) {
        return ST_NOT_EMPTY;
    }

    int ret = st_robustlock_destroy(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    table->pool = NULL;
    table->element_cnt = 0;
    table->inited = 0;

    return ret;
}

static int st_table_run_gc_if_needed(st_table_t *table) {

    st_gc_t *gc = &table->pool->gc;

    if (table->pool->run_gc_periodical == 1) {
        return ST_OK;
    }

    // 9223372036854775808U is half of 2^64.
    if ((uintptr_t)table * 11400714819323198485U > 9223372036854775808U) {
        return ST_OK;
    }

    int ret = st_gc_run(gc);
    if (ret != ST_OK && ret != ST_NO_GC_DATA) {
        return ret;
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

    st_atomic_incr(&pool->table_cnt, 1);

    *table = t;

    return ret;
}

int st_table_release(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_table_pool_t *pool = table->pool;

    int ret = st_table_destroy(table);
    if (ret != ST_OK) {
        return ret;
    }

    st_atomic_decr(&pool->table_cnt, 1);

    return st_slab_obj_free(&pool->slab_pool, table);
}

// this function is only used for gc, other one please use st_table_remove_all.
int st_table_remove_all_for_gc(st_table_t *table) {

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
        st_atomic_decr(&table->element_cnt, 1);

        e = st_owner(n, st_table_element_t, rbnode);

        ret = st_table_release_element(table, e);
        if (ret != ST_OK) {
            goto quit;
        }
    }

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}

int st_table_remove_all(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_rbtree_node_t *n = NULL;
    st_table_element_t *e = NULL;
    st_gc_t *gc = &table->pool->gc;

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        st_robustlock_unlock_err_abort(&gc->lock);
        return ret;
    }

    while (!st_rbtree_is_empty(&table->elements)) {

        n = st_rbtree_left_most(&table->elements);

        st_rbtree_delete(&table->elements, n);
        st_atomic_decr(&table->element_cnt, 1);

        e = st_owner(n, st_table_element_t, rbnode);

        if (st_types_is_table(e->value.type)) {
            st_table_t *t = st_table_get_table_addr_from_value(e->value);

            ret = st_gc_push_to_sweep(&t->pool->gc, &t->gc_head);
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
    st_robustlock_unlock_err_abort(&gc->lock);

    if (ret == ST_OK) {
        return st_table_run_gc_if_needed(table);
    }

    return ret;
}

static int st_table_foreach_elements(st_table_t *table, st_rbtree_node_t *node,
                                     st_table_visit_f visit_func, void *arg) {

    st_rbtree_node_t *sentinel = &table->elements.sentinel;

    if (node == sentinel) {
        return ST_OK;
    }

    int ret = st_table_foreach_elements(table, node->left, visit_func, arg);
    if (ret != ST_OK) {
        return ret;
    }

    st_table_element_t *e = st_owner(node, st_table_element_t, rbnode);

    ret = visit_func(e->value, arg);
    if (ret != ST_OK) {
        return ret;
    }

    return st_table_foreach_elements(table, node->right, visit_func, arg);
}

int st_table_foreach(st_table_t *table, st_table_visit_f visit_func, void *arg) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_foreach_elements(table, table->elements.root, visit_func, arg);

    st_robustlock_unlock_err_abort(&table->lock);

    return ret;
}

int st_table_add_key_value(st_table_t *table, st_str_t key, st_str_t value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value.bytes != NULL && value.len > 0, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;

    int ret = st_table_new_element(table, key, value, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    if (st_types_is_table(value.type)) {
        st_table_t *t = st_table_get_table_addr_from_value(value);
        st_gc_t *gc = &table->pool->gc;

        ret = st_robustlock_lock(&gc->lock);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_table_add_element(table, elem);
        if (ret != ST_OK) {
            st_robustlock_unlock_err_abort(&gc->lock);
            goto quit;
        }

        ret = st_gc_push_to_mark(gc, &t->gc_head);

        st_robustlock_unlock_err_abort(&gc->lock);

    } else {
        ret = st_table_add_element(table, elem);
    }

quit:
    if (ret == ST_OK) {
        return st_table_run_gc_if_needed(table);
    }

    st_table_release_element(table, elem);
    return ret;
}

int st_table_remove_key(st_table_t *table, st_str_t key) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);

    st_table_element_t *removed = NULL;
    st_gc_t *gc = &table->pool->gc;

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_remove_element(table, key, &removed);
    if (ret != ST_OK) {
        st_robustlock_unlock_err_abort(&gc->lock);
        return ret;
    }

    if (st_types_is_table(removed->value.type)) {
        st_table_t *t = st_table_get_table_addr_from_value(removed->value);

        ret = st_gc_push_to_sweep(gc, &t->gc_head);
    }

    st_robustlock_unlock_err_abort(&gc->lock);

    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_release_element(table, removed);
    if (ret != ST_OK) {
        return ret;
    }

    return st_table_run_gc_if_needed(table);
}

// lock table before use the function
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

    return ret;
}

// lock table before use the function
int st_table_iter_next_value(st_table_t *table, st_table_iter_t *iter, st_str_t *value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(iter != NULL, ST_ARG_INVALID);
    st_must(value != NULL, ST_ARG_INVALID);

    st_rbtree_node_t *n = NULL;

    if (*iter == NULL) {
        n = st_rbtree_left_most(&table->elements);
    } else {
        n = st_rbtree_get_next(&table->elements, &(*iter)->rbnode);
    }

    if (n == NULL) {
        return ST_NOT_FOUND;
    }

    st_table_element_t *elem = st_owner(n, st_table_element_t, rbnode);

    *value = elem->value;
    *iter = elem;

    return ST_OK;
}

int st_table_pool_init(st_table_pool_t *pool, int run_gc_periodical) {

    st_must(pool != NULL, ST_ARG_INVALID);

    int ret = st_gc_init(&pool->gc);
    if (ret != ST_OK) {
        return ret;
    }

    pool->table_cnt = 0;
    pool->run_gc_periodical = run_gc_periodical;

    return ret;
}

int st_table_pool_destroy(st_table_pool_t *pool) {

    st_must(pool != NULL, ST_ARG_INVALID);

    int ret = st_gc_destroy(&pool->gc);
    if (ret != ST_OK) {
        return ret;
    }

    pool->table_cnt = 0;

    return ret;
}
