#include "bitmap.h"

#define ST_BITMAP_BITS_PER_WORD (sizeof(uint64_t) * 8)

#define ST_BITMAP_BITS_ALL_SET (~0ULL)

static int in_one_word_(uint32_t start, uint32_t end) {
    st_assert(start < end);

    return start / ST_BITMAP_BITS_PER_WORD == (end - 1) / ST_BITMAP_BITS_PER_WORD;
}

static uint64_t mask_(uint32_t start, uint32_t end) {
    st_assert(in_one_word_(start, end));

    return (ST_BITMAP_BITS_ALL_SET << start)
           & (ST_BITMAP_BITS_ALL_SET >> (ST_BITMAP_BITS_PER_WORD - end));
}

static uint64_t word_bits_value_(uint64_t *bitmap, uint32_t start, uint32_t end, int reverse) {

    st_assert(in_one_word_(start, end));

    uint32_t w_idx = start / ST_BITMAP_BITS_PER_WORD;

    start = start % ST_BITMAP_BITS_PER_WORD;

    end = end % ST_BITMAP_BITS_PER_WORD;
    if (end == 0) {
        end = ST_BITMAP_BITS_PER_WORD;
    }

    if (start == 0 && end == ST_BITMAP_BITS_PER_WORD) {
        return reverse == 0 ? bitmap[w_idx] : ~bitmap[w_idx];
    }

    return reverse == 0 ? bitmap[w_idx] & mask_(start, end)
                        : ~bitmap[w_idx] & mask_(start, end);
}

static uint64_t word_bits_(uint64_t *bitmap, uint32_t start, uint32_t end) {
    return word_bits_value_(bitmap, start, end, 0);
}

static uint64_t word_bits_reverse_(uint64_t *bitmap, uint32_t start, uint32_t end) {
    return word_bits_value_(bitmap, start, end, 1);
}

int st_bitmap_get(uint64_t *bitmap, uint32_t idx) {
    return word_bits_(bitmap, idx, idx + 1) ? 1: 0;
}

void st_bitmap_set(uint64_t *bitmap, uint32_t idx) {
    uint32_t w_idx = idx / ST_BITMAP_BITS_PER_WORD;

    idx = idx % ST_BITMAP_BITS_PER_WORD;

    bitmap[w_idx] |=  mask_(idx, idx + 1);
}

void st_bitmap_clear(uint64_t *bitmap, uint32_t idx) {
    uint32_t w_idx = idx / ST_BITMAP_BITS_PER_WORD;

    idx = idx % ST_BITMAP_BITS_PER_WORD;

    bitmap[w_idx] &= ~mask_(idx, idx + 1);
}

int st_bitmap_are_all_cleared(uint64_t *bitmap, uint32_t nbits)
{
    st_must(bitmap != NULL, ST_ARG_INVALID);

    uint32_t idx = 0;
    uint32_t boundary = ST_BITMAP_BITS_PER_WORD;

    while (idx < nbits) {
        if (boundary > nbits) {
            boundary = nbits;
        }

        if (word_bits_(bitmap, idx, boundary) != 0) {
            return 0;
        }

        idx = boundary;
        boundary += ST_BITMAP_BITS_PER_WORD;
    }

    return 1;
}

int st_bitmap_are_all_set(uint64_t *bitmap, uint32_t nbits)
{
    st_must(bitmap != NULL, ST_ARG_INVALID);

    uint32_t idx = 0;
    uint32_t boundary = ST_BITMAP_BITS_PER_WORD;

    while (idx < nbits) {
        if (boundary > nbits) {
            boundary = nbits;
        }

        if (word_bits_reverse_(bitmap, idx, boundary) != 0) {
            return 0;
        }

        idx = boundary;
        boundary += ST_BITMAP_BITS_PER_WORD;
    }

    return 1;
}

int st_bitmap_equal(uint64_t *bitmap1, uint64_t *bitmap2, uint32_t nbits)
{
    st_must(bitmap1 != NULL, ST_ARG_INVALID);
    st_must(bitmap2 != NULL, ST_ARG_INVALID);

    uint32_t idx = 0;
    uint32_t boundary = ST_BITMAP_BITS_PER_WORD;

    while (idx < nbits) {
        if (boundary > nbits) {
            boundary = nbits;
        }

        if (word_bits_(bitmap1, idx, boundary) != word_bits_(bitmap2, idx, boundary)) {
            return 0;
        }

        idx = boundary;
        boundary += ST_BITMAP_BITS_PER_WORD;
    }

    return 1;
}

int st_bitmap_find_next_bit(uint64_t *bitmap, uint32_t start, uint32_t end, int bit_value)
{
    st_must(bitmap != NULL, ST_ARG_INVALID);
    st_must(start < end, ST_INDEX_OUT_OF_RANGE);
    st_must(bit_value == 1 || bit_value == 0, ST_ARG_INVALID);

    uint64_t word;
    uint32_t idx = start;
    uint32_t boundary = (start / ST_BITMAP_BITS_PER_WORD + 1) * ST_BITMAP_BITS_PER_WORD;
    int reverse = bit_value == 0 ? 1 : 0;

    while (1) {

        if (boundary > end) {
            boundary = end;
        }

        word = word_bits_value_(bitmap, idx, boundary, reverse);
        if (word != 0) {
            break;
        }

        idx = boundary;
        boundary += ST_BITMAP_BITS_PER_WORD;

        if (idx == end) {
            return -1;
        }
    }

    for (int i = idx; i < boundary; i++) {
        word = word_bits_value_(bitmap, i, i + 1, reverse);
        if (word != 0) {
            return i;
        }
    }

    return -1;
}
