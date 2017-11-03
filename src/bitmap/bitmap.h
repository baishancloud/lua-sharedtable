#ifndef _BITMAP_H_INCLUDED_
#define _BITMAP_H_INCLUDED_

#include <stdint.h>
#include <string.h>
#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"

uint64_t st_bitmap_get(uint64_t *bitmap, uint64_t bit_index);

void st_bitmap_set(uint64_t *bitmap, uint64_t bit_index);

void st_bitmap_clear(uint64_t *bitmap, uint64_t bit_index);

int st_bitmap_are_all_cleared(uint64_t *bitmap, uint64_t nbits);

int st_bitmap_are_all_set(uint64_t *bitmap, uint64_t nbits);

int st_bitmap_equal(uint64_t *bitmap1, uint64_t *bitmap2, uint64_t nbits);

int st_bitmap_find_next_bit(uint64_t *bitmap, uint64_t nbits, uint64_t start_index, int bit_value);

static inline int st_bitmap_find_set_bit(uint64_t *bitmap, uint64_t nbits, uint64_t start_index) {
    return st_bitmap_find_next_bit(bitmap, nbits, start_index, 1);
}

static inline int st_bitmap_find_clear_bit(uint64_t *bitmap, uint64_t nbits, uint64_t start_index) {
    return st_bitmap_find_next_bit(bitmap, nbits, start_index, 0);
}

#endif /* _BITMAP_H_INCLUDED_ */
