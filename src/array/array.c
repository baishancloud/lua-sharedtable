#include "array.h"

static int st_array_extend_when_needed(st_array_t *array, size_t incr_cnt);

int st_array_init_static(st_array_t *array, size_t element_size,
        void *start_addr, size_t total_cnt)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(start_addr != NULL, ST_ARG_INVALID);
    st_must(total_cnt > 0, ST_ARG_INVALID);

    array->dynamic = 0;
    array->start_addr = start_addr;
    array->element_size = element_size;
    array->current_cnt = 0;
    array->total_cnt = total_cnt;
    array->inited = 1;

    return ST_OK;
}

int st_array_init_dynamic(st_array_t *array, size_t element_size,
        st_callback_memory_t callback)
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

static int st_array_extend_when_needed(st_array_t *array, size_t incr_cnt)
{
    void *addr;
    size_t capacity;

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

int st_array_insert_many(st_array_t *array, size_t index, void * elements, size_t cnt)
{
    int ret;

    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(elements != NULL, ST_ARG_INVALID);
    st_must(index >= 0 && index <= array->current_cnt, ST_INDEX_OUT_OF_RANGE);

    if (array->dynamic == 1) {
        ret = st_array_extend_when_needed(array, cnt);
        if (ret != ST_OK) {
            return ret;
        }
    }

    if (array->current_cnt + cnt > array->total_cnt) {
        return ST_OUT_OF_MEMORY;
    }

    if (index < array->current_cnt) {
        memmove(st_array_get(array, index + cnt),
                st_array_get(array, index),
                array->element_size * (array->current_cnt - index)
                );
    }

    memcpy(st_array_get(array, index), elements, array->element_size * cnt);
    array->current_cnt += cnt;

    return ST_OK;
}

int st_array_append_many(st_array_t *array, void * elements, size_t cnt)
{
    return st_array_insert_many(array, array->current_cnt, elements, cnt);
}

int st_array_remove_many(st_array_t *array, size_t index, size_t cnt)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(index >= 0 && index + cnt <= array->current_cnt, ST_INDEX_OUT_OF_RANGE);

    if (index + cnt != array->current_cnt) {
        memmove(st_array_get(array, index),
                st_array_get(array, index + cnt),
                array->element_size * (array->current_cnt - (index + cnt))
                );
    }

    array->current_cnt -= cnt;

    return ST_OK;
}

int st_array_sort(st_array_t *array, st_array_compare_f compare_func)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(compare_func != NULL, ST_ARG_INVALID);

    qsort(array->start_addr, array->current_cnt, array->element_size, compare_func);

    return ST_OK;
}

void * st_array_bsearch(st_array_t *array, void *element, st_array_compare_f compare_func)
{

    st_must(array != NULL, NULL);
    st_must(array->inited == 1, NULL);
    st_must(element != NULL, NULL);
    st_must(compare_func != NULL, NULL);

    return bsearch(element, array->start_addr, array->current_cnt, array->element_size, compare_func);
}

void * st_array_indexof(st_array_t *array, void *element, st_array_compare_f compare_func)
{
    st_must(array != NULL, NULL);
    st_must(array->inited == 1, NULL);
    st_must(element != NULL, NULL);
    st_must(compare_func != NULL, NULL);

    void *curr;

    for (int i = 0; i < array->current_cnt; i++) {
        curr = st_array_get(array, i);

        if (compare_func(curr, element) == 0) {
            return curr;
        }
    }

    return NULL;
}
