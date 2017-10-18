#include "array.h"

int st_static_array_init(st_static_array_t *array, int element_size,
        void *start_addr, int total_count)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(start_addr != NULL, ST_ARG_INVALID);
    st_must(total_count > 0, ST_ARG_INVALID);

    memset(array, 0, sizeof(*array));

    array->dynamic = 0;
    array->start_addr = start_addr;
    array->element_size = element_size;
    array->current_cnt = 0;
    array->total_cnt = total_count;
    array->inited = 1;

    return ST_OK;
}

int st_dynamic_array_init(st_dynamic_array_t*array, int element_size,
        st_realloc_f realloc_func)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(realloc_func != NULL, ST_ARG_INVALID);

    memset(array, 0, sizeof(*array));

    array->dynamic = 1;
    array->start_addr = NULL;
    array->element_size = element_size;
    array->current_cnt = 0;
    array->total_cnt = 0;
    array->realloc_func = realloc_func;
    array->inited = 1;

    return ST_OK;
}

int st_array_destroy(st_array_t *array)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);

    // if is static array, you must free memory space by yourself
    if (array->dynamic == 1 && array->start_addr != NULL) {
        array->realloc_func(array->start_addr, 0);
    }

    memset(array, 0, sizeof(*array));

    return ST_OK;
}

static int st_array_maybe_expand(st_array_t *array, int incr_count)
{
    void *addr;
    int capacity;

    if (array->current_cnt + incr_count <= array->total_cnt) {
        return ST_OK;
    }

    incr_count = st_max(incr_count, ST_MIN_ARRAY_SIZE);
    capacity = array->element_size * (array->total_cnt + incr_count);

    addr = array->realloc_func(array->start_addr, capacity);
    if (addr == NULL) {
        return ST_OUT_OF_MEMORY;
    }

    array->start_addr = addr;
    array->total_cnt = array->total_cnt + incr_count;

    return ST_OK;
}

int st_array_insert_many(st_array_t *array, int index, void * elements, int count)
{
    int ret;

    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(elements != NULL, ST_ARG_INVALID);
    st_must(index >= 0 && index <= array->current_cnt, ST_INDEX_OUT_OF_RANGE);

    if (array->dynamic == 1) {
        ret = st_array_maybe_expand(array, count);
        if (ret != ST_OK) {
            return ret;
        }
    }

    if (array->current_cnt + count > array->total_cnt) {
        return ST_OUT_OF_MEMORY;
    }

    if (index < array->current_cnt) {
        memmove(st_array_get(array, index + count),
                st_array_get(array, index),
                array->element_size * (array->current_cnt - index)
                );
    }

    memcpy(st_array_get(array, index), elements, array->element_size * count);
    array->current_cnt += count;

    return ST_OK;
}

int st_array_append_many(st_array_t *array, void * elements, int count)
{
    return st_array_insert_many(array, array->current_cnt, elements, count);
}

int st_array_remove_many(st_array_t *array, int index, int count)
{
    st_must(array != NULL, ST_ARG_INVALID);
    st_must(array->inited == 1, ST_UNINITED);
    st_must(index >= 0 && index + count <= array->current_cnt, ST_INDEX_OUT_OF_RANGE);

    if (index + count != array->current_cnt) {
        memmove(st_array_get(array, index),
                st_array_get(array, index + count),
                array->element_size * (array->current_cnt - (index + count))
                );
    }

    array->current_cnt -= count;

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

        if (memcmp(curr, element, array->element_size) == 0) {
            return curr;
        }
    }

    return NULL;
}
