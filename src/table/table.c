#include "table.h"

int st_table_return_ref(st_table_t *table);

static int st_table_cmp_element(st_rbtree_node_t *a, st_rbtree_node_t *b) {

    st_table_element_t *ea = st_owner(a, st_table_element_t, rbnode);
    st_table_element_t *eb = st_owner(b, st_table_element_t, rbnode);

    return st_str_cmp(&ea->key, &eb->key);
}

static int st_table_new_element(st_table_t *table, st_str_t key,
                                st_str_t val, int is_table_ref, st_table_element_t **new) {

    st_table_element_t *elem = NULL;
    st_slab_pool_t *slab_pool = &table->pool->slab_pool;

    ssize_t size = sizeof(st_table_element_t) + key.len + val.len;

    int ret = st_slab_obj_alloc(slab_pool, size, (void **)&elem);
    if (ret != ST_OK) {
        return ret;
    }

    elem->rbnode = (st_rbtree_node_t)st_rbtree_node_empty;

    st_memcpy(elem->kv_data, key.bytes, key.len);
    elem->key = (st_str_t)st_str_wrap(elem->kv_data, key.len);

    st_memcpy(elem->kv_data + key.len, val.bytes, val.len);
    elem->val = (st_str_t)st_str_wrap(elem->kv_data + key.len, val.len);

    elem->is_table_ref = is_table_ref;

    if (is_table_ref) {
        st_table_t *t = *(st_table_t **)(elem->val.bytes);

        ret = st_refcnt_incr(&t->refcnt, getpid(), 1, NULL, NULL);
        if (ret != ST_OK) {
            st_slab_obj_free(slab_pool, elem);
            return ret;
        }
    }

    *new = elem;

    return ST_OK;
}

static int st_table_release_element(st_table_t *table, st_table_element_t *elem) {

    st_slab_pool_t *slab_pool = &table->pool->slab_pool;

    if (elem->is_table_ref) {
        st_table_t *t = *(st_table_t **)(elem->val.bytes);

        int ret = st_table_return_ref(t);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return st_slab_obj_free(slab_pool, elem);
}

static int st_table_insert_element(st_table_t *table, st_table_element_t *elem, int force) {

    st_table_element_t *old = NULL;

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    st_rbtree_node_t *n = st_rbtree_search_eq(&table->elements, &elem->rbnode);
    if (n != NULL) {
        if (force == 0) {
            st_robustlock_unlock_err_abort(&table->lock);
            return ST_EXISTED;
        } else {
            old = st_owner(n, st_table_element_t, rbnode);
        }
    }

    if (old == NULL) {
        st_rbtree_insert(&table->elements, &elem->rbnode);
    } else {
        st_rbtree_replace(&table->elements, &old->rbnode, &elem->rbnode);
    }

    st_robustlock_unlock_err_abort(&table->lock);

    if (old != NULL) {
        ret = st_table_release_element(table, old);
    }

    return ret;
}

static int st_table_remove_element(st_table_t *table, st_table_element_t *expect) {

    int ret = st_robustlock_lock(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    st_rbtree_node_t *n = st_rbtree_search_eq(&table->elements, &expect->rbnode);
    if (n == NULL) {
        st_robustlock_unlock_err_abort(&table->lock);
        return ST_NOT_FOUND;
    }

    st_table_element_t *elem = st_owner(n, st_table_element_t, rbnode);

    if (expect->val.len > 0 && st_str_cmp(&expect->val, &elem->val) != 0) {
        st_robustlock_unlock_err_abort(&table->lock);
        return ST_NOT_EQUAL;
    }

    st_rbtree_delete(&table->elements, n);

    st_robustlock_unlock_err_abort(&table->lock);

    return st_table_release_element(table, elem);
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

static int st_table_init(st_table_t *table, st_table_pool_t *pool, st_str_t name) {

    st_callback_memory_t callback = {
        .pool = &pool->slab_pool,
        .alloc_with_ret = (st_callback_alloc_with_ret_f)st_slab_obj_alloc,
        .free_with_ret = (st_callback_free_with_ret_f)st_slab_obj_free,
    };

    int ret = st_rbtree_init(&table->elements, st_table_cmp_element);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_init(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_refcnt_init(&table->refcnt, callback);
    if (ret != ST_OK) {
        st_robustlock_destroy(&table->lock);
        return ret;
    }

    st_memcpy(table->name_data, name.bytes, name.len);
    table->name = (st_str_t)st_str_wrap(table->name_data, name.len);

    table->pool = pool;
    table->inited = 1;

    return ST_OK;
}

static int st_table_destroy(st_table_t *table) {

    int64_t refcnt;
    st_rbtree_node_t *n = NULL;
    st_table_element_t *e = NULL;

    int ret = st_refcnt_get_total_refcnt(&table->refcnt, &refcnt);
    if (ret != ST_OK) {
        return ret;
    }

    if (refcnt > 0) {
        return ST_NOT_READY;
    }

    while (!st_rbtree_is_empty(&table->elements)) {

        n = st_rbtree_left_most(&table->elements);
        st_rbtree_delete(&table->elements, n);

        e = st_owner(n, st_table_element_t, rbnode);

        ret = st_table_release_element(table, e);
        if (ret != ST_OK) {
            return ret;
        }
    }

    ret = st_robustlock_destroy(&table->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_refcnt_destroy(&table->refcnt);
    if (ret != ST_OK) {
        return ret;
    }

    table->pool = NULL;
    table->inited = 0;

    return ST_OK;
}

int st_table_new(st_table_pool_t *pool, st_str_t name, st_table_t **new) {

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(new != NULL, ST_ARG_INVALID);
    st_must(name.bytes != NULL, ST_ARG_INVALID);
    st_must(name.len > 0 && name.len <= ST_TABLE_MAX_NAME_LEN, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;
    st_table_t *t = NULL;
    st_table_t *root = &pool->root_table;

    int ret = st_slab_obj_alloc(&pool->slab_pool, sizeof(st_table_t), (void **)&t);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_init(t, pool, name);
    if (ret != ST_OK) {
        st_slab_obj_free(&pool->slab_pool, t);
        return ret;
    }

    ret = st_table_new_element(root, name, (st_str_t)st_str_wrap(&t, sizeof(t)), 1, &elem);
    if (ret != ST_OK) {
        st_slab_obj_free(&pool->slab_pool, t);
        return ret;
    }

    ret = st_table_insert_element(root, elem, 0);
    if (ret != ST_OK) {
        // release_element will decr table ref, then will free table memory
        st_table_release_element(root, elem);
        return ret;
    }

    *new = t;

    return ST_OK;
}

int st_table_release(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    st_table_t *root = &table->pool->root_table;

    st_table_element_t elem = {
        .key = table->name,
        .val = st_str_wrap(&table, sizeof(table)),
    };

    return st_table_remove_element(root, &elem);
}

int st_table_get_ref(st_table_pool_t *pool, st_str_t name, st_table_t **table) {

    st_must(pool != NULL, ST_ARG_INVALID);
    st_must(table != NULL, ST_ARG_INVALID);
    st_must(name.bytes != NULL, ST_ARG_INVALID);
    st_must(name.len > 0 && name.len <= ST_TABLE_MAX_NAME_LEN, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;
    st_table_t *root = &pool->root_table;

    int ret = st_robustlock_lock(&root->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_get_element(root, name, &elem);
    if (ret != ST_OK) {
        goto quit;
    }

    *table = *(st_table_t **)(elem->val.bytes);

    ret = st_refcnt_incr(&(*table)->refcnt, getpid(), 1, NULL, NULL);

quit:
    st_robustlock_unlock_err_abort(&root->lock);
    return ret;
}

int st_table_return_ref(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    int64_t refcnt;
    st_slab_pool_t *slab_pool = &table->pool->slab_pool;

    int ret = st_refcnt_decr(&table->refcnt, getpid(), 1, NULL, &refcnt);
    if (ret != ST_OK) {
        return ret;
    }

    if (refcnt == 0) {
        ret = st_table_destroy(table);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_slab_obj_free(slab_pool, table);
        if (ret != ST_OK) {
            return ret;
        }
    }

    return ret;
}

int st_table_insert_value(st_table_t *table, st_str_t key, st_str_t val, int is_table_ref,
                          int force) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);
    st_must(val.bytes != NULL && val.len > 0, ST_ARG_INVALID);
    st_must(is_table_ref == 0 || is_table_ref == 1, ST_ARG_INVALID);
    st_must(force == 0 || force == 1, ST_ARG_INVALID);

    st_table_element_t *elem = NULL;

    int ret = st_table_new_element(table, key, val, is_table_ref, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_insert_element(table, elem, force);
    if (ret != ST_OK) {
        st_table_release_element(table, elem);
    }

    return ret;
}

int st_table_remove_value(st_table_t *table, st_str_t key) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);

    st_table_element_t elem = {.key = key};

    return st_table_remove_element(table, &elem);
}

int st_table_lock(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    return st_robustlock_lock(&table->lock);
}

int st_table_get_value(st_table_t *table, st_str_t key, st_str_t *value) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(value != NULL, ST_ARG_INVALID);
    st_must(key.bytes != NULL && key.len > 0, ST_ARG_INVALID);

    st_table_element_t *elem;

    int ret = st_table_get_element(table, key, &elem);
    if (ret != ST_OK) {
        return ret;
    }

    *value = elem->val;

    return ST_OK;
}

static int st_table_get_all_elements_recursive(st_table_t *table, st_rbtree_node_t *node,
        st_list_t *all_elements) {

    st_rbtree_node_t *sentinel = &table->elements.sentinel;

    if (node == sentinel) {
        return ST_OK;
    }

    int ret = st_table_get_all_elements_recursive(table, node->left, all_elements);
    if (ret != ST_OK) {
        return ret;
    }

    st_table_element_t *e = st_owner(node, st_table_element_t, rbnode);
    st_list_insert_last(all_elements, &e->lnode);

    ret = st_table_get_all_elements_recursive(table, node->right, all_elements);
    if (ret != ST_OK) {
        return ret;
    }

    return ST_OK;
}

int st_table_get_all_elements(st_table_t *table, st_list_t *all_elements) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);
    st_must(all_elements != NULL, ST_ARG_INVALID);

    return st_table_get_all_elements_recursive(table, table->elements.root, all_elements);
}

int st_table_unlock(st_table_t *table) {

    st_must(table != NULL, ST_ARG_INVALID);
    st_must(table->inited, ST_UNINITED);

    return st_robustlock_unlock(&table->lock);
}

static int st_table_clean_process_ref_recursive(st_table_t *table, st_rbtree_node_t *node,
        pid_t pid, st_list_t *removing_elements) {

    int64_t process_refcnt;
    int64_t total_refcnt;

    st_rbtree_node_t *sentinel = &table->elements.sentinel;

    if (node == sentinel) {
        return ST_OK;
    }

    int ret = st_table_clean_process_ref_recursive(table, node->left, pid, removing_elements);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_clean_process_ref_recursive(table, node->right, pid, removing_elements);
    if (ret != ST_OK) {
        return ret;
    }

    st_table_element_t *e = st_owner(node, st_table_element_t, rbnode);
    st_table_t *t = *(st_table_t **)(e->val.bytes);

    ret = st_refcnt_get_process_refcnt(&t->refcnt, pid, &process_refcnt);
    if (ret == ST_NOT_FOUND) {
        return ST_OK;
    }

    ret = st_refcnt_decr(&t->refcnt, pid, process_refcnt, NULL, &total_refcnt);
    if (ret != ST_OK) {
        return ret;
    }

    if (total_refcnt == 0) {
        st_list_insert_last(removing_elements, &e->lnode);
    }

    return ST_OK;
}

int st_table_clean_process_ref(st_table_pool_t *pool, pid_t pid) {

    st_must(pool != NULL, ST_ARG_INVALID);

    st_table_t *t = NULL;
    st_table_element_t *e = NULL;
    st_table_element_t *next = NULL;
    st_slab_pool_t *slab_pool = &pool->slab_pool;

    st_table_t *root = &pool->root_table;
    st_list_t removing_elements = ST_LIST_INIT(removing_elements);

    int ret = st_robustlock_lock(&root->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_table_clean_process_ref_recursive(root, root->elements.root, pid, &removing_elements);
    if (ret != ST_OK) {
        goto quit;
    }

    st_list_for_each_entry_safe(e, next, &removing_elements, lnode) {

        t = *(st_table_t **)(e->val.bytes);

        ret = st_table_destroy(t);
        if (ret != ST_OK) {
            goto quit;
        }

        ret = st_slab_obj_free(slab_pool, t);
        if (ret != ST_OK) {
            goto quit;
        }

        st_rbtree_delete(&root->elements, &e->rbnode);

        ret = st_slab_obj_free(slab_pool, e);
        if (ret != ST_OK) {
            goto quit;
        }
    }

quit:
    st_robustlock_unlock_err_abort(&root->lock);
    return ret;
}

int st_table_pool_init(st_table_pool_t *pool) {
    st_must(pool != NULL, ST_ARG_INVALID);

    return st_table_init(&pool->root_table, pool, (st_str_t)st_str_const("_st_root_table"));
}

int st_table_pool_destroy(st_table_pool_t *pool) {
    st_must(pool != NULL, ST_ARG_INVALID);

    return st_table_destroy(&pool->root_table);
}
