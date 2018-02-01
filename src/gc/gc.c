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

    if (!st_types_is_table(value.type)) {
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

static int st_gc_mark_reachable_tables(st_gc_t *gc, int *marked_cnt) {

    int ret;
    st_table_t *t = NULL;
    st_list_t *node = NULL;
    st_gc_head_t *gc_head = NULL;

    while (*marked_cnt < gc->mark_cnt_per_step) {

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

        // 1 is current table.
        *marked_cnt = *marked_cnt + 1 + st_atomic_load(&t->element_cnt);
    }

    return ST_OK;
}

static int st_gc_table_to_sweep_queue(st_str_t value, st_list_t *sweep_queue) {

    if (!st_types_is_table(value.type)) {
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

static int st_gc_mark_garbage_tables(st_gc_t *gc, int is_prev, int *marked_cnt) {

    int ret;
    st_gc_head_t *gc_head = NULL;
    st_table_t *t = NULL;
    st_list_t *node = NULL, *sweep_queue = NULL;

    if (is_prev) {
        sweep_queue = &gc->prev_sweep_queue;
    } else {
        sweep_queue = &gc->sweep_queue;
    }

    while (*marked_cnt < gc->mark_cnt_per_step) {

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
                st_list_insert_last(&gc->remained_queue, node);
            }

        } else {
            t = st_owner(gc_head, st_table_t, gc_head);

            gc_head->mark = st_gc_status_garbage(gc);
            st_list_insert_last(&gc->garbage_queue, node);

            ret = st_table_foreach(t, (st_table_visit_f)st_gc_table_to_sweep_queue, sweep_queue);
            if (ret != ST_OK) {
                return ret;
            }

            *marked_cnt += st_atomic_load(&t->element_cnt);
        }

        // add 1 is current table.
        *marked_cnt += 1;
    }

    return ST_OK;
}

static int st_gc_mark_tables(st_gc_t *gc) {

    int err;
    int marked_cnt = 0;
    int64_t start_usec = 0, end_usec = 0;

    int ret = st_time_in_usec(&start_usec);
    if (ret != ST_OK) {
        return ret;
    }

    // mark in mark_queue
    ret = st_gc_mark_reachable_tables(gc, &marked_cnt);
    if (ret == ST_OK) {
        goto quit;
    } else if (ret != ST_EMPTY) {
        return ret;
    }

    // mark in prev_sweep_queue
    ret = st_gc_mark_garbage_tables(gc, 1, &marked_cnt);
    if (ret == ST_OK) {
        goto quit;
    } else if (ret != ST_EMPTY) {
        return ret;
    }

    // mark in sweep_queue
    ret = st_gc_mark_garbage_tables(gc, 0, &marked_cnt);
    if (ret == ST_OK) {
        goto quit;
    } else if (ret != ST_EMPTY) {
        return ret;
    }

quit:
    err = st_time_in_usec(&end_usec);
    if (err != ST_OK) {
        return err;
    }

    if (marked_cnt > 0) {
        float usec = st_max((float)(end_usec - start_usec) / marked_cnt, 0.01);
        gc->mark_cnt_per_step = st_max(ST_GC_MAX_TIME_IN_USEC / usec, 1);
    }

    dd("mark use usec: %d, marked_cnt: %d, next mark_cnt_per_step: %d",
       (int)(end_usec - start_usec), marked_cnt, gc->mark_cnt_per_step);

    return ret;
}

static int st_gc_free_tables(st_gc_t *gc) {

    int freed_cnt = 0;
    int64_t start_usec = 0, end_usec = 0;
    st_list_t *node = NULL;
    st_gc_head_t *gc_head = NULL;
    st_table_t *t = NULL;

    int ret = st_time_in_usec(&start_usec);
    if (ret != ST_OK) {
        return ret;
    }

    while (freed_cnt < gc->free_cnt_per_step) {

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

        freed_cnt = freed_cnt + 1 + st_atomic_load(&t->element_cnt);
    }

    ret = st_time_in_usec(&end_usec);
    if (ret != ST_OK) {
        return ret;
    }

    if (freed_cnt > 0) {
        float usec = st_max((float)(end_usec - start_usec) / freed_cnt, 0.1);
        gc->free_cnt_per_step = st_max(ST_GC_MAX_TIME_IN_USEC / usec, 1);
    }

    dd("free use usec: %d, freed_cnt: %d, next free_cnt_per_step: %d",
       (int)(end_usec - start_usec), freed_cnt, gc->free_cnt_per_step);

    return ST_OK;
}

int st_gc_run(st_gc_t *gc) {

    st_must(gc != NULL, ST_ARG_INVALID);

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    if (!gc->begin) {

        if (st_list_empty(&gc->sweep_queue) && st_list_empty(&gc->prev_sweep_queue)) {
            ret = ST_NO_GC_DATA;
            goto quit;
        }

        ret = st_time_in_usec(&gc->start_usec);
        if (ret != ST_OK) {
            goto quit;
        }

        st_gc_roots_to_mark_queue(gc);

        gc->begin = 1;
    }

    ret = st_gc_mark_tables(gc);
    if (ret != ST_EMPTY) {
        goto quit;
    }

    ret = st_gc_free_tables(gc);
    if (ret != ST_EMPTY) {
        goto quit;
    }

    st_list_join(&gc->prev_sweep_queue, &gc->remained_queue);

    ret = st_time_in_usec(&gc->end_usec);
    if (ret != ST_OK) {
        goto quit;
    }

    st_atomic_incr(&gc->round, 4);
    gc->begin = 0;

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

    ssize_t idx;

    int ret = st_robustlock_lock(&gc->lock);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_bsearch_right(&gc->roots, &gc_head, NULL, &idx);
    if (ret == ST_OK) {
        ret = ST_EXISTED;
        goto quit;
    } else if (ret != ST_NOT_FOUND) {
        goto quit;
    }

    ret = st_array_insert(&gc->roots, idx, &gc_head);

quit:
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

int st_gc_init(st_gc_t *gc) {
    st_must(gc != NULL, ST_ARG_INVALID);

    gc->round = 0;
    gc->begin = 0;
    gc->mark_cnt_per_step = 100;
    gc->free_cnt_per_step = 50;

    st_list_init(&gc->mark_queue);
    st_list_init(&gc->prev_sweep_queue);
    st_list_init(&gc->sweep_queue);
    st_list_init(&gc->garbage_queue);
    st_list_init(&gc->remained_queue);

    int ret = st_time_in_usec(&gc->start_usec);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_time_in_usec(&gc->end_usec);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_init_static(&gc->roots, sizeof(st_gc_head_t *),
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
            !st_list_empty(&gc->garbage_queue) ||
            !st_list_empty(&gc->remained_queue)) {

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
