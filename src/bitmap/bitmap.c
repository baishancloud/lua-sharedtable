#include "bitmap.h"

#define ST_BITMAP_BITS_PER_WORD (sizeof(uint64_t) * 8)

#define ST_BITMAP_BITS_ALL_SET (~0ULL)

static uint64_t mask_(uint32_t start, uint32_t end) {

    return (ST_BITMAP_BITS_ALL_SET << start) \
        & (ST_BITMAP_BITS_ALL_SET >> (ST_BITMAP_BITS_PER_WORD - end));
}

static uint64_t word_(uint64_t word, uint32_t start, uint32_t end) {
    return word & mask_(start, end);
}

int st_bitmap_get(uint64_t *bitmap, uint32_t idx) {
    uint32_t w_idx = idx / ST_BITMAP_BITS_PER_WORD;
    uint32_t idx_in_w = idx % ST_BITMAP_BITS_PER_WORD;

    return word_(bitmap[w_idx], idx_in_w, idx_in_w + 1) ? 1: 0;
}

void st_bitmap_set(uint64_t *bitmap, uint32_t idx) {
    uint32_t w_idx = idx / ST_BITMAP_BITS_PER_WORD;
    uint32_t idx_in_w = idx % ST_BITMAP_BITS_PER_WORD;

    bitmap[w_idx] |=  mask_(idx_in_w, idx_in_w + 1);
}

void st_bitmap_clear(uint64_t *bitmap, uint32_t idx) {
    uint32_t w_idx = idx / ST_BITMAP_BITS_PER_WORD;
    uint32_t idx_in_w = idx % ST_BITMAP_BITS_PER_WORD;

    bitmap[w_idx] &= ~mask_(idx_in_w, idx_in_w + 1);
}

int st_bitmap_are_all_cleared(uint64_t *bitmap, uint32_t nbits)
{
    int i;
    uint32_t remain = nbits % ST_BITMAP_BITS_PER_WORD;

    st_must(bitmap != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (bitmap[i] != 0) {
            return 0;
        }
    }

    if (remain != 0 && word_(bitmap[i], 0, remain) != 0) {
        return 0;
    }

    return 1;
}

int st_bitmap_are_all_set(uint64_t *bitmap, uint32_t nbits)
{
    int i;
    uint32_t remain = nbits % ST_BITMAP_BITS_PER_WORD;

    st_must(bitmap != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (~bitmap[i] != 0) {
            return 0;
        }
    }

    if (remain != 0 && word_(~bitmap[i], 0, remain) != 0) {
        return 0;
    }

    return 1;
}

int st_bitmap_equal(uint64_t *bitmap1, uint64_t *bitmap2, uint32_t nbits)
{
    int i;
    uint32_t remain = nbits % ST_BITMAP_BITS_PER_WORD;

    st_must(bitmap1 != NULL, ST_ARG_INVALID);
    st_must(bitmap2 != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (bitmap1[i] != bitmap2[i]) {
            return 0;
        }
    }

    if (remain != 0 && word_(bitmap1[i], 0, remain) != word_(bitmap2[i], 0, remain)) {
        return 0;
    }

    return 1;
}

int st_bitmap_find_next_bit(uint64_t *bitmap, uint32_t start, uint32_t end, int bit_value)
{
    uint64_t word;
    int start_w_idx;
    int end_w_idx;
    int start_in_w;
    int end_in_w;

    st_must(bitmap != NULL, ST_ARG_INVALID);
    st_must(start < end, ST_INDEX_OUT_OF_RANGE);
    st_must(bit_value == 1 || bit_value == 0, ST_ARG_INVALID);

    start_w_idx = start / ST_BITMAP_BITS_PER_WORD;

    end_w_idx = end / ST_BITMAP_BITS_PER_WORD;
    if (end % ST_BITMAP_BITS_PER_WORD != 0) {
        end_w_idx += 1;
    }

    for (int i = start_w_idx; i < end_w_idx; i++) {

        word = bitmap[i];

        if (bit_value == 0) {
            //if find 0 bit, reverse the word, so can use find 1 bit flow
            word = ~word;
        }

        start_in_w = 0;
        end_in_w = ST_BITMAP_BITS_PER_WORD;

        if (i == start_w_idx) {
            start_in_w = start % ST_BITMAP_BITS_PER_WORD;
            word = word_(word, start_in_w, ST_BITMAP_BITS_PER_WORD);
        }

        if (i == end_w_idx - 1) {
            if (end % ST_BITMAP_BITS_PER_WORD != 0) {
                end_in_w = end % ST_BITMAP_BITS_PER_WORD;
            }
            word = word_(word, 0, end_in_w);
        }

        if (word == 0) {
            continue;
        }

        for (int j = start_in_w; j < end_in_w; j++) {
            if (word_(word, j, j + 1) != 0) {
                return i * ST_BITMAP_BITS_PER_WORD + j;
            }
        }
    }

    return -1;
}
