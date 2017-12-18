#include "table.h"

int st_table_return_reference(st_table_t *table);

static int st_table_cmp_element(st_rbtree_node_t *a, st_rbtree_node_t *b) {

    st_table_element_t *ea = st_owner(a, st_table_element_t, rbnode);
    st_table_element_t *eb = st_owner(b, st_table_element_t, rbnode);

    return st_str_cmp(&ea->key, &eb->key);
}

static int st_table_new_element(st_table_t *table, st_str_t key,
                                st_str_t value, int is_table_ref, st_table_element_t **new) {

    st_table_element_t *elem = NULL;

    ssize_t size = sizeof(st_table_element_t) + key.len + value.len;

    int ret = st_slab_obj_alloc(table->slab_pool, size, (void **)&elem);
    if (ret != ST_OK) {
        return ret;
    }

    elem->rbnode = (st_rbtree_node_t)st_rbtree_node_empty;

    st_memcpy(elem->kv_data, key.bytes, key.len);
    elem->key = (st_str_t)st_str_wrap(elem->kv_data, key.len);

    st_memcpy(elem->kv_data + key.len, value.bytes, value.len);
    elem->value = (st_str_t)st_str_wrap(elem->kv_data + key.len, value.len);

    elem->is_table_ref = is_table_ref;

    *new = elem;

    return ST_OK;
}

static int st_table_release_element(st_table_t *table, st_table_element_t *elem) {

    return st_slab_obj_free(table->slab_pool, elem);
}

static int st_table_insert_element(st_table_t *table, st_table_element_t *elem) {

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

static int st_table_init(st_table_t *table, st_slab_pool_t *slab_pool) {

    int ret = st_rbtree_init(&table->elements, st_table_cmp_element);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_init(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    table->slab_pool = slab_pool;
    /* new table hold reference */
    table->refcnt = 1;
    table->inited = 1;

    return ST_OK;
}

static int st_table_destroy(st_table_t *table) {

    int64_t ret;
    st_rbtree_node_t *n = NULL;
    st_table_element_t *e = NULL;

    if (st_atomic_load(&table->refcnt) != 0) {
        return ST_NOT_READY;
    }

    while (!st_rbtree_is_empty(&table->elements)) {

        n = st_rbtree_left_most(&table->elements);
        st_rbtree_delete(&table->elements, n);

        e = st_owner(n, st_table_element_t, rbnode);

        if (e->is_table_ref) {
            st_table_t *t = *(st_table_t **)(e->value.bytes);

            ret = st_table_return_reference(t);
            if (ret != ST_OK) {
                return ret;
            }
        }

        ret = st_table_release_element(table, e);
        if (ret != ST_OK) {
            return ret;
        }
    }

    ret = st_robustlock_destroy(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    table->slab_pool = NULL;
    table->refcnt = 0;
    table->inited = 0;

    return ST_OK;
}

int st_table_get_reference(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_atomic_incr(&table->refcnt, 1);

    return ST_OK;
}

int st_table_return_reference(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    int64_t refcnt = st_atomic_decr(&table->refcnt, 1);

    if (refcnt == 0) {
        st_slab_pool_t *slab_pool = table->slab_pool;

        int ret = st_table_destroy(table);
        if (ret != ST_OK) {
            return ret;
        }

        return st_slab_obj_free(slab_pool, table);
    }

    return ST_OK;
}

int st_table_new(st_slab_pool_t *slab_pool, st_table_t **new) {

    st_must(slab_pool != NULL, ST_ARG_INVALID);
    st_must(new != NULL, ST_ARG_INVALID);

    st_table_t *t = NULL;

    int ret = st_slab_obj_alloc(slab_pool, sizeof(st_table_t), (void **)&t);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_init(t, slab_pool);
    if (ret != ST_OK) {
        st_slab_obj_free(slab_pool, t);
        return ret;
    }

    *new = t;

    return ST_OK;
}

int st_table_release(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    return st_table_return_reference(table);
}

int st_table_insert_value(st_table_t *table, st_str_t key, st_str_t value, int is_table_ref) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(value.bytes != NULL && value.len > 0, ST_ARG_INVALID);
    st_must(is_table_ref == 0 || is_table_ref == 1, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;
    st_table_t *t = NULL;

    int ret = st_table_new_element(table, key, value, is_table_ref, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    if (is_table_ref) {
        t = *(st_table_t **)(elem->value.bytes);

        ret = st_table_get_reference(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    ret = st_table_insert_element(table, elem);
    if (ret != ST_OK) {
        st_table_release_element(table, elem);

        if (is_table_ref) {
            st_table_return_reference(t);
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

    if (removed->is_table_ref) {
        st_table_t *t = *(st_table_t **)(removed->value.bytes);
        ret = st_table_return_reference(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return st_table_release_element(table, removed);
}

int st_table_lock(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    return st_robustlock_lock(&table->lock);
}

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

static int st_table_get_elements_recursive(st_table_t *table, st_rbtree_node_t *node,
        st_list_t *all) {

    st_rbtree_node_t *sentinel = &table->elements.sentinel;

    if (node == sentinel) {
        return ST_OK;
    }

    int ret = st_table_get_elements_recursive(table, node->left, all);
    if (ret != ST_OK) {
        return ret;
    }

    st_table_element_t *e = st_owner(node, st_table_element_t, rbnode);
    st_list_insert_last(all, &e->lnode);

    ret = st_table_get_elements_recursive(table, node->right, all);
    if (ret != ST_OK) {
        return ret;
    }

    return ST_OK;
}

int st_table_get_all_sorted_elements(st_table_t *table, st_list_t *all) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(all != NULL, ST_ARG_INVALID);

    return st_table_get_elements_recursive(table, table->elements.root, all);
}

int st_table_unlock(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    return st_robustlock_unlock(&table->lock);
}

int st_table_get_table_with_reference(st_table_t *table, st_str_t key, st_table_t **sub) {

    st_table_t *t = NULL;
    st_str_t value;

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_get_value(table, key, &value);
    if (ret != ST_OK) {
        goto quit;
    }

    t = *(st_table_t **)(value.bytes);

    ret = st_table_get_reference(t);
    if (ret != ST_OK) {
        goto quit;
    }

    *sub = t;

quit:
    st_robustlock_unlock_err_abort(&table->lock);
    return ret;
}
