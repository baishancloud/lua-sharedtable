#ifndef _ARRAY_H_INCLUDED_
#define _ARRAY_H_INCLUDED_

#include <stdint.h>
#include <string.h>
#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"
#include "inc/callback.h"

typedef struct st_array_s st_array_t;

typedef int (*st_array_compare_f) (const void *a, const void *b);

struct st_array_s {
    void * start_addr;

    size_t element_size;
    size_t current_cnt;
    size_t total_cnt;

    int dynamic;

    st_callback_memory_t callback;
    int inited;
};

#define ST_ARRAY_MIN_SIZE 64

#define arg2(xx, x, ...) (x)


static inline int st_array_is_empty(st_array_t *array) {
    return array->current_cnt == 0;
}

static inline int st_array_is_full(st_array_t *array) {
    return array->current_cnt == array->total_cnt;
}

static inline int st_array_get_index(st_array_t *array, void *ptr) {
    return (ptr - array->start_addr) / array->element_size;
}

static inline void * st_array_get(st_array_t *array, size_t index) {
    return array->start_addr + array->element_size * index;
}

int st_array_init_static(st_array_t *array, size_t element_size,
        void *start_addr, size_t total_cnt);

int st_array_init_dynamic(st_array_t *array, size_t element_size,
        st_callback_memory_t callback);

int st_array_destroy(st_array_t *array);


#define st_array_insert(array, idx, elts, ...) \
        st_array_insert_many(array, idx, elts, arg2(xx, ##__VA_ARGS__, 1))

int st_array_insert_many(st_array_t *array, size_t index, void * elements, size_t cnt);


#define st_array_remove(array, idx, ...) \
        st_array_remove_many(array, idx, arg2(xx, ##__VA_ARGS__, 1))

int st_array_remove_many(st_array_t *array, size_t index, size_t cnt);


#define st_array_append(array, elts, ...) \
        st_array_append_many(array, elts, arg2(xx, ##__VA_ARGS__, 1))

int st_array_append_many(st_array_t *array, void * elements, size_t cnt);

int st_array_sort(st_array_t *array, st_array_compare_f compare_func);

void * st_array_bsearch(st_array_t *array, void *element, st_array_compare_f compare_func);

void * st_array_indexof(st_array_t *array, void *element, st_array_compare_f compare_func);

#endif /* _ARRAY_H_INCLUDED_ */
