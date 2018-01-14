#include "table/table.h"
#include "gc.h"

static int st_gc_cmp_gc_head(const void *a, const void *b) {
    st_gc_head_t *a_addr = *(st_gc_head_t **)a;
    st_gc_head_t *b_addr = *(st_gc_head_t **)b;

    return st_cmp((uintptr_t)a_addr, (uintptr_t)b_addr);
}

static void st_gc_roots_to_mark_queue(st_gc_t *gc) {

    st_gc_head_t *gc_head = NULL;

    for (int i = 0; i < st_array_current_cnt(&gc->roots); i++) {
        gc_head = *(st_gc_head_t **)st_array_get(&gc->roots, i);

        st_list_insert_last(&gc->mark_queue, &gc_head->mark_lnode);
    }
}

static int st_gc_table_to_mark_queue(st_str_t value, st_list_t *mark_queue) {

    if (!st_table_value_is_table(value)) {
        return ST_OK;
    }

    st_table_t *t = st_table_get_table_addr_from_value(value);

    st_gc_t *gc = &t->pool->gc;
    st_gc_head_t *gc_head = &t->gc_head;

    if (gc_head->mark == st_gc_status_reachable(gc)) {
        return ST_OK;
    }

    if (st_list_is_inited(&gc_head->mark_lnode)) {
        return ST_OK;
    }

    st_list_insert_last(mark_queue, &gc_head->mark_lnode);

    return ST_OK;
}

static int st_gc_mark_reachable_tables(st_gc_t *gc) {

    int64_t start_tm = 0, curr_tm = 0;
    st_table_t *t = NULL;
    st_list_t *node = NULL;
    st_gc_head_t *gc_head = NULL;

    int ret = st_time_in_usec(&start_tm);
    if (ret != ST_OK) {
        return ret;
    }

    do {
        node = st_list_pop_first(&gc->mark_queue);
        if (node == NULL) {
            return ST_EMPTY;
        }

        gc_head = st_owner(node, st_gc_head_t, mark_lnode);
        gc_head->mark = st_gc_status_reachable(gc);

        t = st_owner(gc_head, st_table_t, gc_head);

        ret = st_table_foreach(t, (st_table_visit_f)st_gc_table_to_mark_queue, &gc->mark_queue);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_time_in_usec(&curr_tm);
        if (ret != ST_OK) {
            return ret;
        }

    } while (curr_tm - start_tm < ST_GC_MAX_TIME_IN_USEC);

    return ST_OK;
}

static int st_gc_table_to_sweep_queue(st_str_t value, st_list_t *sweep_queue) {

    if (!st_table_value_is_table(value)) {
        return ST_OK;
    }

    st_table_t *t = st_table_get_table_addr_from_value(value);

    st_gc_t *gc = &t->pool->gc;
    st_gc_head_t *gc_head = &t->gc_head;

    if (gc_head->mark == st_gc_status_reachable(gc) || gc_head->mark == st_gc_status_garbage(gc)) {
        return ST_OK;
    }

    if (st_list_is_inited(&gc_head->sweep_lnode)) {
        return ST_OK;
    }

    st_list_insert_last(sweep_queue, &gc_head->sweep_lnode);

    return ST_OK;
}

static int st_gc_mark_garbage_tables(st_gc_t *gc, int is_prev) {

    int64_t start_tm = 0, curr_tm = 0;
    st_gc_head_t *gc_head = NULL;
    st_table_t *t = NULL;
    st_list_t *node = NULL, *sweep_queue = NULL;

    if (is_prev) {
        sweep_queue = &gc->prev_sweep_queue;
    } else {
        sweep_queue = &gc->sweep_queue;
    }

    int ret = st_time_in_usec(&start_tm);
    if (ret != ST_OK) {
        return ret;
    }

    do {
        node = st_list_pop_first(sweep_queue);
        if (node == NULL) {
            return ST_EMPTY;
        }

        gc_head = st_owner(node, st_gc_head_t, sweep_lnode);

        if (gc_head->mark == st_gc_status_garbage(gc)) {
            //garbage table should be in garbage_queue.
            return ST_STATE_INVALID;

        } else if (gc_head->mark == st_gc_status_reachable(gc)) {

            if (!is_prev) {
                st_list_insert_last(&gc->prev_sweep_queue, node);
            }

        } else {
            t = st_owner(gc_head, st_table_t, gc_head);

            gc_head->mark = st_gc_status_garbage(gc);
            st_list_insert_last(&gc->garbage_queue, node);

            ret = st_table_foreach(t, (st_table_visit_f)st_gc_table_to_sweep_queue, sweep_queue);
            if (ret != ST_OK) {
                return ret;
            }
        }

        ret = st_time_in_usec(&curr_tm);
        if (ret != ST_OK) {
            return ret;
        }

    } while (curr_tm - start_tm < ST_GC_MAX_TIME_IN_USEC);

    return ST_OK;
}

static int st_gc_sweep_tables(st_gc_t *gc) {

    int64_t start_tm = 0, curr_tm = 0;
    st_list_t *node = NULL;
    st_gc_head_t *gc_head = NULL;
    st_table_t *t = NULL;

    int ret = st_time_in_usec(&start_tm);
    if (ret != ST_OK) {
        return ret;
    }

    do {
        node = st_list_pop_first(&gc->garbage_queue);
        if (node == NULL) {
            return ST_EMPTY;
        }

        gc_head = st_owner(node, st_gc_head_t, sweep_lnode);
        t = st_owner(gc_head, st_table_t, gc_head);

        ret = st_table_remove_all_for_gc(t);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_table_release(t);
        if (ret != ST_OK) {
            return ret;
        }

        ret = st_time_in_usec(&curr_tm);
        if (ret != ST_OK) {
            return ret;
        }

    } while (curr_tm - start_tm < ST_GC_MAX_TIME_IN_USEC);

    return ST_OK;
}

int st_gc_run(st_gc_t *gc) {

    st_must(gc != NULL, ST_ARG_INVALID);

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    switch (gc->phase) {

        case ST_GC_PHASE_INITIAL:

            if (gettimeofday(&gc->start_tm, NULL) != 0) {
                ret = errno;
                goto quit;
            }

            if (st_list_empty(&gc->sweep_queue) && st_list_empty(&gc->prev_sweep_queue)) {
                ret = ST_NO_GC_DATA;
                goto quit;
            }

            st_gc_roots_to_mark_queue(gc);

            gc->phase = ST_GC_PHASE_MARK_PREV_SWEEP;

        case ST_GC_PHASE_MARK_PREV_SWEEP:

            ret = st_gc_mark_reachable_tables(gc);
            if (ret != ST_EMPTY) {
                goto quit;
            }

            ret = st_gc_mark_garbage_tables(gc, 1);
            if (ret != ST_EMPTY) {
                goto quit;
            }

            gc->phase = ST_GC_PHASE_MARK_SWEEP;
            ret = ST_OK;
            break;

        case ST_GC_PHASE_MARK_SWEEP:

            ret = st_gc_mark_reachable_tables(gc);
            if (ret != ST_EMPTY) {
                goto quit;
            }

            ret = st_gc_mark_garbage_tables(gc, 0);
            if (ret != ST_EMPTY) {
                goto quit;
            }

            gc->phase = ST_GC_PHASE_SWEEP_GARBAGE;
            ret = ST_OK;
            break;

        case ST_GC_PHASE_SWEEP_GARBAGE:

            ret = st_gc_sweep_tables(gc);
            if (ret != ST_EMPTY) {
                goto quit;
            }

            if (gettimeofday(&gc->end_tm, NULL) != 0) {
                ret = errno;
            }

            st_atomic_incr(&gc->round, 4);
            gc->phase = ST_GC_PHASE_INITIAL;
            ret = ST_OK;
    }

quit:
    st_robustlock_unlock_err_abort(&gc->lock);
    return ret;
}

int st_gc_push_to_mark(st_gc_t *gc, st_gc_head_t *gc_head) {

    st_must(gc != NULL, ST_ARG_INVALID);
    st_must(gc_head != NULL, ST_ARG_INVALID);

    if (gc_head->mark == st_gc_status_reachable(gc)) {
        return ST_OK;
    }

    if (st_list_is_inited(&gc_head->mark_lnode)) {
        return ST_OK;
    }

    st_list_insert_last(&gc->mark_queue, &gc_head->mark_lnode);

    return ST_OK;
}

int st_gc_push_to_sweep(st_gc_t *gc, st_gc_head_t *gc_head) {

    st_must(gc != NULL, ST_ARG_INVALID);
    st_must(gc_head != NULL, ST_ARG_INVALID);

    // maybe the table in prev_sweep_queue.
    // if you do not move it from prev_sweep_queue to sweep_queue.
    // you maybe lose deleting table chance, so delete the table from queue first.
    if (st_list_is_inited(&gc_head->sweep_lnode)) {
        st_list_remove(&gc_head->sweep_lnode);
    }

    st_list_insert_last(&gc->sweep_queue, &gc_head->sweep_lnode);

    return ST_OK;
}

int st_gc_add_root(st_gc_t *gc, st_gc_head_t *gc_head) {

    st_must(gc != NULL, ST_ARG_INVALID);
    st_must(gc_head != NULL, ST_ARG_INVALID);

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_append(&gc->roots, &gc_head);

    st_robustlock_unlock_err_abort(&gc->lock);
    return ret;
}

int st_gc_remove_root(st_gc_t *gc, st_gc_head_t *gc_head) {

    ssize_t idx;

    st_must(gc != NULL, ST_ARG_INVALID);
    st_must(gc_head != NULL, ST_ARG_INVALID);

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_bsearch_left(&gc->roots, &gc_head, NULL, &idx);
    if (ret != ST_OK) {
        goto quit;
    }

    ret = st_array_remove(&gc->roots, idx);

quit:
    st_robustlock_unlock_err_abort(&gc->lock);
    return ret;
}

int st_gc_init(st_gc_t *gc, int run_in_periodical) {
    st_must(gc != NULL, ST_ARG_INVALID);
    st_must(run_in_periodical == 0 || run_in_periodical == 1, ST_ARG_INVALID);

    gc->round = 0;
    gc->phase = ST_GC_PHASE_INITIAL;
    gc->run_in_periodical = run_in_periodical;

    st_list_init(&gc->mark_queue);
    st_list_init(&gc->prev_sweep_queue);
    st_list_init(&gc->sweep_queue);
    st_list_init(&gc->garbage_queue);

    if (gettimeofday(&gc->start_tm, NULL) != 0) {
        return errno;
    }

    if (gettimeofday(&gc->end_tm, NULL) != 0) {
        return errno;
    }

    int ret = st_array_init_static(&gc->roots, sizeof(st_gc_head_t *),
                                   gc->roots_data, ST_GC_MAX_ROOTS, st_gc_cmp_gc_head);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_robustlock_init(&gc->lock);
    if (ret != ST_OK) {
        st_array_destroy(&gc->roots);
        return ret;
    }

    return ret;
}

int st_gc_destroy(st_gc_t *gc) {

    int ret, err = ST_OK;
    st_must(gc != NULL, ST_ARG_INVALID);

    while (1) {
        ret = st_gc_run(gc);
        if (ret == ST_NO_GC_DATA) {
            break;
        } else if (ret != ST_OK) {
            return ret;
        }
    }

    if (!st_list_empty(&gc->mark_queue) ||
            !st_list_empty(&gc->prev_sweep_queue) ||
            !st_list_empty(&gc->sweep_queue) ||
            !st_list_empty(&gc->garbage_queue)) {

        return ST_STATE_INVALID;
    }

    ret = st_array_destroy(&gc->roots);
    if (ret != ST_OK) {
        derr("st_array_destroy error %d\n", ret);
        err = ret;
    }

    ret = st_robustlock_destroy(&gc->lock);
    if (ret != ST_OK) {
        derr("st_robustlock_destroy gc lock error %d\n", ret);
        err = err == ST_OK ? ret : err;
    }

    return err;
}
