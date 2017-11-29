<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
#   Table of Content

- [Name](#name)
- [Status](#status)
- [Synopsis](#synopsis)
- [Description](#description)
- [Methods](#methods)
  - [st_array_init_static](#st_array_init_static)
  - [st_array_init_dynamic](#st_array_init_dynamic)
  - [st_array_destroy](#st_array_destroy)
  - [st_array_insert](#st_array_insert)
  - [st_array_remove](#st_array_remove)
  - [st_array_append](#st_array_append)
  - [st_array_sort](#st_array_sort)
  - [st_array_indexof](#st_array_indexof)
  - [st_array_bsearch_left](#st_array_bsearch_left)
  - [st_array_bsearch_right](#st_array_bsearch_right)
  - [st_array_is_empty](#st_array_is_empty)
  - [st_array_is_full](#st_array_is_full)
  - [st_array_get](#st_array_get)
  - [st_array_get_index](#st_array_get_index)
  - [st_array_current_cnt](#st_array_current_cnt)
- [Author](#author)
- [Copyright and License](#copyright-and-license)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Name

array

Support static and dynamic array, you can insert, remove, sort, search array.

# Status

This library is considered production ready.

# Synopsis

Usage with static array:

```

#include "array.h"

int main()
{
    st_array_t array = {0};
    int array_buf[10];

    int ret = st_array_init_static(&array, sizeof(int), array_buf, 10, NULL);
    if (ret != ST_OK) {
        return ret;
    }

    int append_v = 2;

    ret = st_array_append(&array, &append_v);
    if (ret != ST_OK) {
        return ret;
    }

    int insert_v = 12;

    ret = st_array_insert(&array, 1, &insert_v);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_remove(&array, 0);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_destroy(&array);
    if (ret != ST_OK) {
        return ret;
    }

    return ret;
}

```

Usage with dynamic array:

```

#include <stdlib.h>
#include "array.h"

int compare(const void *a, const void *b)
{
    int m = *(int *) a;
    int n = *(int *) b;

    return m-n;
}

int main()
{
    st_array_t array = {0};
    int append_buf[10] = {9, 8, 0, 1, 2, 3, 4, 5, 6, 7};

    st_callback_memory_t callback = {.pool = NULL, .realloc = realloc, .free = free,}

    int ret = st_array_init_dynamic(&array, sizeof(int), callback, compare);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_append(&array, append_buf, 10);
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_sort(&array, NULL);
    if (ret != ST_OK) {
        return ret;
    }

    int value = 6;
    ssize_t idx;

    ret = st_array_bsearch_left(&array, &value, NULL, &idx),
    if (ret != ST_OK) {
        return ret;
    }

    ret = st_array_bsearch_right(&array, &value, NULL, &idx),
    if (ret != ST_OK) {
        return ret;
    }

    return ret;
}
```

# Description

Support static and dynamic array, can insert, remove, sort, search etc operation.

#   Methods

## st_array_init_static

**syntax**:

`int st_array_init_static(st_array_t *array, ssize_t element_size,
        void *start_addr, ssize_t total_cnt, st_array_compare_f compare);`

Init static array with already allocated array space,
static array not support space dynamic extend,
it just manage the space start from start_addr.

**arguments**:

-   `array`:
    array instance pointer, this function will init array struct members.

-   `element_size`:
    element size.

-   `start_addr`:
    array space start address, all the elements will store in this space.

-   `total_cnt`:
    array can support total element count.

-   `compare`:
    compare function to compare array element, it will be used for sort, search.
    you can set NULL, if you do not need.

**return**:

if succ return ST_OK, else return errno.

## st_array_init_dynamic

**syntax**:

`int st_array_init_dynamic(st_array_t *array, ssize_t element_size,
        st_callback_memory_t callback, st_array_compare_f compare)`

Init dynamic array, array memory space will be allocated from callback's realloc.

**arguments**:

-   `array`:
    array instance pointer.

-   `element_size`:
    element size.

-   `callback`:
    you must init the callback's realloc and free member, it will be used.

-   `compare`:
    compare function to compare array element, it will be used for sort, search.
    you can set NULL, if you do not need.

**return**:

if succ return ST_OK, else return errno.

## st_array_destroy

**syntax**:

`int st_array_destroy(st_array_t *array)`

Destroy array, release array resource.

**arguments**:

-   `array`:
    array instance pointer.

**return**:

if succ return ST_OK, else return errno.

## st_array_insert

**syntax**:

`st_array_insert(array, idx, elts, ...)`

The api is macro, you can insert one or some elements to array.

**arguments**:

-   `array`:
    array instance pointer.

-   `idx`:
    which position you want to insert elements.

-   `elts`:
    insert elements, you can insert one element or some. depend on the fourth arg.

-   `...`:
    the fourth arg is insert elements num,
    if only insert one, no need the fourth arg.

**return**:

if succ return ST_OK, else return errno.

## st_array_remove

**syntax**:

`st_array_remove(array, idx, ...)`

The api is macro, you can remove one or some elements from array.

**arguments**:

-   `array`:
    array instance pointer.

-   `idx`:
    which position you want to remove elements.

-   `...`:
    the third arg is remove elements num,
    if only remove one, no need the third arg.

**return**:

if succ return ST_OK, else return errno.

## st_array_append

**syntax**:
`st_array_append(array, elts, ...)`

The api is macro, you can append one or some elements to array.

**arguments**:

-   `array`:
    array instance pointer.

-   `elts`:
    append elements, you can append one element or some. depend on the third arg.

-   `...`:
    the third arg is append elements num,
    if only append one, no need the third arg.

**return**:

if succ return ST_OK, else return errno.

## st_array_sort

**syntax**:
`int st_array_sort(st_array_t *array, st_array_compare_f compare)`

Sort array elements depend on compare function.

**arguments**:

-   `array`:
    array instance pointer.

-   `compare`:
    compare function, if you set NULL, then sort will use default compare,
    which get from init function.

**return**:

if succ return ST_OK, else return errno.

## st_array_indexof

**syntax**:

`int st_array_indexof(st_array_t *array, void *element,
                     st_array_compare_f compare, ssize_t *idx)`

Find element from unsorted array.

**arguments**:

-   `array`:
    array instance pointer.

-   `element`:
    the element has the value which want to find from array.

-   `compare`:
    compare function, if you set NULL, then sort will use default compare,
    which get from init function.

-   `idx`:
    found the element idx.

**return**:

if found return ST_OK, if not found return ST_NOT_FOUND, others error will return errno.

## st_array_bsearch_left

**syntax**:

`int st_array_bsearch_left(st_array_t *array, void *element,
                           st_array_compare_f compare, ssize_t *idx)`

Locate the insertion pointer for element in array to maintain sorted order.

If found, it returns ST_OK and idx is set to the first elt == `element`.
If not found, it returns ST_NOT_FOUND and idx is set to the first elt > `element`.

**arguments**:

-   `array`:
    array instance pointer.

-   `element`:
    the element has the value which want to find from array.

-   `compare`:
    compare function, if you set NULL, then sort will use default compare,
    which get from init function.

-   `idx`:
    found the idx.

**return**:

if found return ST_OK, if not found return ST_NOT_FOUND, others error will return errno.

## st_array_bsearch_right

**syntax**:

`int st_array_bsearch_right(st_array_t *array, void *element,
                           st_array_compare_f compare, ssize_t *idx)`

Similar to st_array_bsearch_left(), but returns an insertion pointer which comes
after (to the right of) any existing entries of element in array.

If found, it returns ST_OK and idx is set to the position after the last elt == `element`.
If not found, it returns ST_NOT_FOUND and idx is set to the first elt > `element`.
Thus st_array_get(x, idx) never equals `element`.

**arguments**:

-   `array`:
    array instance pointer.

-   `element`:
    the element has the value which want to find from array.

-   `compare`:
    compare function, if you set NULL, then sort will use default compare,
    which get from init function.

-   `idx`:
    found the idx.

**return**:

if found return ST_OK, if not found return ST_NOT_FOUND, others error will return errno.

## st_array_is_empty

**syntax**:

`int st_array_is_empty(st_array_t *array)`

Whether Array is empty.

**arguments**:

-   `array`:
    array instance pointer.

**return**:

if array is empty, return 1, else return 0.

## st_array_is_full

**syntax**:

`int st_array_is_full(st_array_t *array)`

Whether array is full.

**arguments**:

-   `array`:
    array instance pointer.

**return**:

if array is full, return 1, else return 0.

## st_array_get

**syntax**:

`void * st_array_get(st_array_t *array, ssize_t idx)`

Get element from array by index.

**arguments**:

-   `array`:
    array instance pointer.

-   `idx`:
    array index.

**return**:

return the element address.

## st_array_get_index

**syntax**:

`ssize_t st_array_get_index(st_array_t *array, void *element)`

Get the element index in array.

**arguments**:

-   `array`:
    array instance pointer.

-   `element`:
    element in array.

**return**:

return the element index.

## st_array_current_cnt

**syntax**:

`ssize_t st_array_current_cnt(st_array_t *array)`

Get the current element num in array.

**arguments**:

-   `array`:
    array instance pointer.

**return**:

return the array current element num.

# Author

cc (陈闯) <chuang.chen@baishancloud.com>

# Copyright and License

The MIT License (MIT)

Copyright (c) 2017 cc (陈闯) <chuang.chen@baishancloud.com>
