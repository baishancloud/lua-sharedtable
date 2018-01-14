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
    e->key = (st_str_t)st_str_wrap(e->kv_data, key.len);

    st_memcpy(e->kv_data + key.len, value.bytes, value.len);
    e->value = (st_str_t)st_str_wrap(e->kv_data + key.len, value.len);

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

    st_rbtree_node_t *n = st_rbtree_search_eq(&table->elements, &elem->rbnode);
    if (n != NULL) {
        ret = ST_EXISTED;
        goto quit;
    }

    st_rbtree_insert(&table->elements, &elem->rbnode);

    table->element_count++;

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

static int st_table_get_next_element(st_table_t *table, st_str_t key, st_table_element_t **elem) {

    st_table_element_t tmp = {.key = key};

    st_rbtree_node_t *n = st_rbtree_search_next(&table->elements, &tmp.rbnode);
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
    table->element_count--;

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
    table->element_count = 0;
    table->inited = 1;

    return ret;
}

static int st_table_destroy(st_table_t *table) {

    if (table->element_count != 0) {
        return ST_NOT_EMPTY;
    }

    int ret = st_robustlock_destroy(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    table->pool = NULL;
    table->element_count = 0;
    table->inited = 0;

    return ret;
}

static int st_table_run_gc_if_needed(st_table_t *table) {

    st_gc_t *gc = &table->pool->gc;

    if (gc->run_in_periodical == 1) {
        return ST_OK;
    }

    // 9223372036854775808U is half of 2^64.
    if ((uintptr_t)table * 11400714819323198485U > 9223372036854775808U) {
        return ST_OK;
    }

    int ret = st_gc_run(gc, 1);
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

    ret = st_table_run_gc_if_needed(t);
    if (ret != ST_OK) {
        st_table_destroy(t);
        st_slab_obj_free(slab_pool, t);
        return ret;
    }

    st_atomic_incr(&pool->table_count, 1);

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

    st_atomic_decr(&pool->table_count, 1);

    return st_slab_obj_free(&pool->slab_pool, table);
}

// this function is only used for gc, other one please use st_table_clear.
int st_table_clear_elements(st_table_t *table, int cleared_to_gc) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(cleared_to_gc == 0 || cleared_to_gc == 1, ST_ARG_INVALID);

    st_rbtree_node_t *n = NULL;
    st_table_element_t *e = NULL;

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    while (!st_rbtree_is_empty(&table->elements)) {

        n = st_rbtree_left_most(&table->elements);

        st_rbtree_delete(&table->elements, n);
        table->element_count--;

        e = st_owner(n, st_table_element_t, rbnode);

        if (cleared_to_gc && st_table_value_is_table(e->value)) {
            st_table_t *t = st_table_get_table_addr_from_value(e->value);

            ret = st_gc_push_to_gc_queue(&t->pool->gc, &t->gc_head);
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

int st_table_clear(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_gc_t *gc = &table->pool->gc;

    // avoid gc mark next_candidate tables in the same time.
    int ret = st_robustlock_lock(&gc->barrier_lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_clear_elements(table, 1);

    st_robustlock_unlock_err_abort(&gc->barrier_lock);

    if (ret != ST_OK) {
        return ret;
    }

    return st_table_run_gc_if_needed(table);
}

static int st_table_traverse_elements(st_table_t *table, st_rbtree_node_t *node,
                                      st_table_visit_f visit_func, void *arg) {

    st_rbtree_node_t *sentinel = &table->elements.sentinel;

    if (node == sentinel) {
        return ST_OK;
    }

    st_table_element_t *e = st_owner(node, st_table_element_t, rbnode);

    int ret = visit_func(e->value, arg);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_traverse_elements(table, node->left, visit_func, arg);
    if (ret != ST_OK) {
        return ret;
    }

    return st_table_traverse_elements(table, node->right, visit_func, arg);
}

int st_table_traverse(st_table_t *table, st_table_visit_f visit_func, void *arg) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_traverse_elements(table, table->elements.root, visit_func, arg);

    st_robustlock_unlock_err_abort(&table->lock);

    return ret;
}

int st_table_add_value(st_table_t *table, st_str_t key, st_str_t value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value.bytes != NULL && value.len > ST_TABLE_VALUE_TYPE_LEN, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;

    int ret = st_table_new_element(table, key, value, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_add_element(table, elem);
    if (ret != ST_OK) {
        st_table_release_element(table, elem);
    }

    return ret;
}

int st_table_remove_value(st_table_t *table, st_str_t key) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);

    st_table_element_t *removed = NULL;
    st_gc_t *gc = &table->pool->gc;

    // avoid gc mark next_candidate tables in the same time.
    int ret = st_robustlock_lock(&gc->barrier_lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_remove_element(table, key, &removed);
    if (ret != ST_OK) {
        st_robustlock_unlock_err_abort(&gc->barrier_lock);
        return ret;
    }

    if (st_table_value_is_table(removed->value)) {
        st_table_t *t = st_table_get_table_addr_from_value(removed->value);

        ret = st_gc_push_to_gc_queue(gc, &t->gc_head);
    }

    st_robustlock_unlock_err_abort(&gc->barrier_lock);

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
int st_table_get_next_value(st_table_t *table, st_str_t key, st_str_t *value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value != NULL, ST_ARG_INVALID);

    st_table_element_t *elem;

    int ret = st_table_get_next_element(table, key, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    *value = elem->value;

    return ret;
}

int st_table_pool_init(st_table_pool_t *pool, int run_gc_periodical) {

    st_must(pool != NULL, ST_ARG_INVALID);

    int ret = st_gc_init(&pool->gc, run_gc_periodical);
    if (ret != ST_OK) {
        return ret;
    }

    pool->table_count = 0;

    return ret;
}

int st_table_pool_destroy(st_table_pool_t *pool) {

    st_must(pool != NULL, ST_ARG_INVALID);

    int ret = st_gc_destroy(&pool->gc);
    if (ret != ST_OK) {
        return ret;
    }

    pool->table_count = 0;

    return ret;
}
