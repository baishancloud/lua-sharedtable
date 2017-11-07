#ifndef _BITMAP_H_INCLUDED_
#define _BITMAP_H_INCLUDED_

#include <stdint.h>
#include <string.h>
#include "inc/err.h"
#include "inc/log.h"
#include "inc/util.h"

int st_bitmap_get(uint64_t *bitmap, uint32_t idx);

void st_bitmap_set(uint64_t *bitmap, uint32_t idx);

void st_bitmap_clear(uint64_t *bitmap, uint32_t idx);

int st_bitmap_are_all_cleared(uint64_t *bitmap, uint32_t nbits);

int st_bitmap_are_all_set(uint64_t *bitmap, uint32_t nbits);

int st_bitmap_equal(uint64_t *bitmap1, uint64_t *bitmap2, uint32_t nbits);

int st_bitmap_find_next_bit(uint64_t *bitmap, uint32_t start, uint32_t end, int bit_value);

static inline int st_bitmap_find_set_bit(uint64_t *bitmap, uint32_t start, uint32_t end) {
    return st_bitmap_find_next_bit(bitmap, start, end, 1);
}

static inline int st_bitmap_find_clear_bit(uint64_t *bitmap, uint32_t start, uint32_t end) {
    return st_bitmap_find_next_bit(bitmap, start, end, 0);
}

#endif /* _BITMAP_H_INCLUDED_ */
