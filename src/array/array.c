#include "array.h"

static int st_array_extend_when_needed(st_array_t *array, ssize_t incr_cnt);

int st_array_init_static(st_array_t *array, ssize_t element_size,
        void *start_addr, ssize_t total_cnt, st_array_compare_f compare)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(start_addr != NULL, ST_ARG_INVALID);
    st_must(total_cnt > 0, ST_ARG_INVALID);

    array->dynamic = 0;
    array->start_addr = start_addr;
    array->element_size = element_size;
    array->current_cnt = 0;
    array->total_cnt = total_cnt;
    array->compare = compare;
    array->inited = 1;

    return ST_OK;
}

int st_array_init_dynamic(st_array_t *array, ssize_t element_size,
        st_callback_memory_t callback, st_array_compare_f compare)
{
    int ret;
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(callback.free != NULL && callback.realloc != NULL, ST_ARG_INVALID);

    array->dynamic = 1;
    array->start_addr = NULL;
    array->element_size = element_size;
    array->current_cnt = 0;
    array->total_cnt = 0;
    array->callback = callback;
    array->compare = compare;

    ret = st_array_extend_when_needed(array, ST_ARRAY_MIN_SIZE);
    if (ret != ST_OK) {
        return ret;
    }

    array->inited = 1;

    return ST_OK;
}

int st_array_destroy(st_array_t *array)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);

    // if is static array, you must free memory space by yourself
    if (array->dynamic == 1 && array->start_addr != NULL) {
        array->callback.free(array->callback.pool, array->start_addr);
    }

    memset(array, 0, sizeof(*array));

    return ST_OK;
}

static int st_array_extend_when_needed(st_array_t *array, ssize_t incr_cnt)
{
    void *addr;
    ssize_t capacity;

    if (array->current_cnt + incr_cnt <= array->total_cnt) {
        return ST_OK;
    }

    incr_cnt = st_max(incr_cnt, ST_ARRAY_MIN_SIZE);
    capacity = array->element_size * (array->total_cnt + incr_cnt);

    addr = array->callback.realloc(array->callback.pool, array->start_addr, capacity);
    if (addr == NULL) {
        return ST_OUT_OF_MEMORY;
    }

    array->start_addr = addr;
    array->total_cnt = array->total_cnt + incr_cnt;

    return ST_OK;
}

int st_array_insert_many(st_array_t *array, ssize_t idx, void *elements, ssize_t cnt)
{
    int ret;

    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(elements != NULL, ST_ARG_INVALID);
    st_must(idx >= 0 && idx <= array->current_cnt, ST_INDEX_OUT_OF_RANGE);

    if (array->dynamic == 1) {
        ret = st_array_extend_when_needed(array, cnt);
        if (ret != ST_OK) {
            return ret;
        }
    }

    if (array->current_cnt + cnt > array->total_cnt) {
        return ST_OUT_OF_MEMORY;
    }

    if (idx < array->current_cnt) {
        memmove(st_array_get(array, idx + cnt),
                st_array_get(array, idx),
                array->element_size * (array->current_cnt - idx)
                );
    }

    memcpy(st_array_get(array, idx), elements, array->element_size * cnt);
    array->current_cnt += cnt;

    return ST_OK;
}

int st_array_append_many(st_array_t *array, void *elements, ssize_t cnt)
{
    return st_array_insert_many(array, array->current_cnt, elements, cnt);
}

int st_array_remove_many(st_array_t *array, ssize_t idx, ssize_t cnt)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(idx >= 0 && idx + cnt <= array->current_cnt, ST_INDEX_OUT_OF_RANGE);

    if (idx + cnt != array->current_cnt) {
        memmove(st_array_get(array, idx),
                st_array_get(array, idx + cnt),
                array->element_size * (array->current_cnt - (idx + cnt))
                );
    }

    array->current_cnt -= cnt;

    return ST_OK;
}

int st_array_sort(st_array_t *array, st_array_compare_f compare)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);

    st_array_compare_f cmp = compare != NULL ? compare : array->compare;
    st_must(cmp != NULL, ST_ARG_INVALID);

    qsort(array->start_addr, array->current_cnt, array->element_size, cmp);

    return ST_OK;
}

int st_array_indexof(st_array_t *array, void *element,
        st_array_compare_f compare, ssize_t *idx)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(element != NULL, ST_ARG_INVALID);

    st_array_compare_f cmp = compare != NULL ? compare : array->compare;
    st_must(cmp != NULL, ST_ARG_INVALID);

    for (ssize_t i = 0; i < array->current_cnt; i++) {

        if (cmp(element, st_array_get(array, i)) == 0) {
            *idx = i;
            return ST_OK;
        }
    }

    return ST_NOT_FOUND;
}

int st_array_bsearch_left(st_array_t *array, void *element,
        st_array_compare_f compare, ssize_t *idx)
{
    /*
     * If found, it returns ST_OK and idx is set to the first elt == `element`.
     * If not found, it returns ST_NOT_FOUND and idx is set to the first elt > `element`.
     */

    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(element != NULL, ST_ARG_INVALID);

    st_array_compare_f cmp = compare != NULL ? compare : array->compare;
    st_must(cmp != NULL, ST_ARG_INVALID);

    int cret;
    int ret = ST_NOT_FOUND;
    ssize_t mid, start, end;

    start = 0;
    end = array->current_cnt;

    while (start < end) {
        mid = (start + end) / 2;

        cret = cmp(element, st_array_get(array, mid));
        if (cret > 0) {
            start = mid + 1;
        } else {
            if (cret == 0) {
                ret = ST_OK;
            }
            end = mid;
        }
    }

    *idx = start;

    return ret;
}

int st_array_bsearch_right(st_array_t *array, void *element,
        st_array_compare_f compare, ssize_t *idx)
{
    /*
     * If found, it returns ST_OK and idx is set to the position AFTER the last elt == `element`.
     * If not found, it returns ST_NOT_FOUND and idx is set to the first elt > `element`.
     * Thus st_array_get(x, idx) never equals `element`.
     */

    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(element != NULL, ST_ARG_INVALID);

    st_array_compare_f cmp = compare != NULL ? compare : array->compare;
    st_must(cmp != NULL, ST_ARG_INVALID);

    int cret;
    int ret = ST_NOT_FOUND;
    ssize_t mid, start, end;

    start = 0;
    end = array->current_cnt;

    while (start < end) {
        mid = (start + end) / 2;

        cret = cmp(element, st_array_get(array, mid));
        if (cret < 0) {
            end = mid;
        } else {
            if (cret == 0) {
                ret = ST_OK;
            }
            start = mid + 1;
        }
    }

    *idx = start;

    return ret;
}
