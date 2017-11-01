#ifndef _BITMAP_H_INCLUDED_
#define _BITMAP_H_INCLUDED_

#include <stdint.h>
#include <string.h>
#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"

uint32_t st_bitmap_get_bit(uint32_t *bitmap, uint32_t bit_index);

void st_bitmap_set_bit(uint32_t *bitmap, uint32_t bit_index);

void st_bitmap_clear_bit(uint32_t *bitmap, uint32_t bit_index);

int st_bitmap_are_all_cleared(uint32_t *bitmap, uint32_t nbits);

int st_bitmap_are_all_set(uint32_t *bitmap, uint32_t nbits);

int st_bitmap_equal(uint32_t *bitmap1, uint32_t *bitmap2, uint32_t nbits);

int st_bitmap_find_next_bit(uint32_t *bitmap, uint32_t nbits, uint32_t start_index, int bit_value);

static inline int st_bitmap_find_set_bit(uint32_t *bitmap, uint32_t nbits, uint32_t start_index) {
    return st_bitmap_find_next_bit(bitmap, nbits, start_index, 1);
}

static inline int st_bitmap_find_clear_bit(uint32_t *bitmap, uint32_t nbits, uint32_t start_index) {
    return st_bitmap_find_next_bit(bitmap, nbits, start_index, 0);
}

#endif /* _BITMAP_H_INCLUDED_ */
