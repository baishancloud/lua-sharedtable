#include "refcnt.h"

static int st_refcnt_cmp_process(st_rbtree_node_t *a, st_rbtree_node_t *b) {
    st_refcnt_process_t *pa = st_owner(a, st_refcnt_process_t, rbnode);
    st_refcnt_process_t *pb = st_owner(b, st_refcnt_process_t, rbnode);

    return st_cmp(pa->pid, pb->pid);
}

static int st_refcnt_new_process(st_refcnt_t *refcnt, pid_t pid, st_refcnt_process_t **process) {

    st_refcnt_process_t *tmp = NULL;
    st_callback_memory_t *cb = &refcnt->callback;

    int ret = cb->alloc_with_ret(cb->pool, sizeof(st_refcnt_process_t), (void **)&tmp);
    if (ret != ST_OK) {
        return ret;
    }

    *tmp = (st_refcnt_process_t){
        .rbnode = (st_rbtree_node_t)st_rbtree_node_empty,
        .pid = pid,
        .cnt = 0,
    };

    *process = tmp;

    return ST_OK;
}

static int st_refcnt_release_process(st_refcnt_t *refcnt, st_refcnt_process_t *process) {
    st_callback_memory_t *cb = &refcnt->callback;

    return cb->free_with_ret(cb->pool, process);
}

static int st_refcnt_get_process(st_refcnt_t *refcnt, pid_t pid, st_refcnt_process_t **process) {

    st_refcnt_process_t tmp = {.pid = pid};

    st_rbtree_node_t *n = st_rbtree_search_eq(&refcnt->processes, &tmp.rbnode);
    if (n == NULL) {
        return ST_NOT_FOUND;
    }

    *process = st_owner(n, st_refcnt_process_t, rbnode);

    return ST_OK;
}

int st_refcnt_init(st_refcnt_t *refcnt, st_callback_memory_t callback) {
    st_must(refcnt != NULL, ST_ARG_INVALID);
    st_must(callback.alloc_with_ret != NULL, ST_ARG_INVALID);
    st_must(callback.free_with_ret != NULL, ST_ARG_INVALID);

    int ret = st_rbtree_init(&refcnt->processes, st_refcnt_cmp_process);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_init(&refcnt->lock);
    if (ret != ST_OK) {
        return ret;
    }

    refcnt->callback = callback;
    refcnt->total_cnt = 0;

    return ret;
}

int st_refcnt_destroy(st_refcnt_t *refcnt) {
    st_must(refcnt != NULL, ST_ARG_INVALID);

    int ret;
    st_rbtree_node_t *n = NULL;
    st_refcnt_process_t *process = NULL;

    while (!st_rbtree_is_empty(&refcnt->processes)) {

        n = st_rbtree_left_most(&refcnt->processes);

        st_rbtree_delete(&refcnt->processes, n);

        process = st_owner(n, st_refcnt_process_t, rbnode);

        ret = st_refcnt_release_process(refcnt, process);
        if (ret != ST_OK) {
            return ret;
        }
    }

    ret = st_robustlock_destroy(&refcnt->lock);
    if (ret != ST_OK) {
        return ret;
    }

    refcnt->total_cnt = 0;

    return ret;
}

int st_refcnt_get_process_refcnt(st_refcnt_t *refcnt, pid_t pid, int64_t *cnt) {

    st_must(refcnt != NULL, ST_ARG_INVALID);
    st_must(cnt != NULL, ST_ARG_INVALID);

    st_refcnt_process_t *process = NULL;

    int ret = st_robustlock_lock(&refcnt->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_refcnt_get_process(refcnt, pid, &process);
    if (ret != ST_OK) {
        goto quit;
    }

    *cnt = process->cnt;

quit:
    st_robustlock_unlock_err_abort(&refcnt->lock);
    return ret;
}

int st_refcnt_get_total_refcnt(st_refcnt_t *refcnt, int64_t *cnt) {

    st_must(refcnt != NULL, ST_ARG_INVALID);
    st_must(cnt != NULL, ST_ARG_INVALID);

    int ret = st_robustlock_lock(&refcnt->lock);
    if (ret != ST_OK) {
        return ret;
    }

    *cnt = refcnt->total_cnt;

    st_robustlock_unlock_err_abort(&refcnt->lock);
    return ret;
}

int st_refcnt_incr(st_refcnt_t *refcnt, pid_t pid, int64_t cnt, int64_t *process_refcnt,
                   int64_t *total_refcnt) {

    st_must(refcnt != NULL, ST_ARG_INVALID);

    st_refcnt_process_t *process = NULL;

    int ret = st_robustlock_lock(&refcnt->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_refcnt_get_process(refcnt, pid, &process);

    if (ret == ST_NOT_FOUND) {

        ret = st_refcnt_new_process(refcnt, pid, &process);
        if (ret != ST_OK) {
            goto quit;
        }

        st_rbtree_insert(&refcnt->processes, &process->rbnode);

    } else if (ret != ST_OK) {
        goto quit;
    }

    process->cnt += cnt;
    refcnt->total_cnt += cnt;

    if (process_refcnt != NULL) {
        *process_refcnt = process->cnt;
    }

    if (total_refcnt != NULL) {
        *total_refcnt = refcnt->total_cnt;
    }

quit:
    st_robustlock_unlock_err_abort(&refcnt->lock);
    return ret;
}

int st_refcnt_decr(st_refcnt_t *refcnt, pid_t pid, int64_t cnt, int64_t *process_refcnt,
                   int64_t *total_refcnt) {

    st_must(refcnt != NULL, ST_ARG_INVALID);

    st_refcnt_process_t *process = NULL;

    int ret = st_robustlock_lock(&refcnt->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_refcnt_get_process(refcnt, pid, &process);
    if (ret != ST_OK) {
        goto quit;
    }

    if (cnt > process->cnt) {
        ret = ST_OUT_OF_RANGE;
        goto quit;
    }

    process->cnt -= cnt;
    refcnt->total_cnt -= cnt;

    if (process_refcnt != NULL) {
        *process_refcnt = process->cnt;
    }

    if (total_refcnt != NULL) {
        *total_refcnt = refcnt->total_cnt;
    }

    if (process->cnt == 0) {
        st_rbtree_delete(&refcnt->processes, &process->rbnode);

        ret = st_refcnt_release_process(refcnt, process);
    }

quit:
    st_robustlock_unlock_err_abort(&refcnt->lock);
    return ret;
}
