#include "bitmap.h"

#define ST_BITMAP_BITS_PER_WORD (sizeof(uint64_t) * 8)

static uint64_t bit_mask_(uint64_t bit_index) {
    uint64_t index_in_word = bit_index % ST_BITMAP_BITS_PER_WORD;

    return 1ULL << index_in_word;
}

uint64_t st_bitmap_get(uint64_t *bitmap, uint64_t bit_index) {
    uint64_t word_index = bit_index / ST_BITMAP_BITS_PER_WORD;

    return bitmap[word_index] & bit_mask_(bit_index) ? 1 : 0;
}

void st_bitmap_set(uint64_t *bitmap, uint64_t bit_index) {
    uint64_t word_index = bit_index / ST_BITMAP_BITS_PER_WORD;

    bitmap[word_index] |= bit_mask_(bit_index);
}

void st_bitmap_clear(uint64_t *bitmap, uint64_t bit_index) {
    uint64_t word_index = bit_index / ST_BITMAP_BITS_PER_WORD;

    bitmap[word_index] &= ~bit_mask_(bit_index);
}

static uint64_t first_word_mask_(uint64_t start_index) {
    uint64_t index_in_word = start_index % ST_BITMAP_BITS_PER_WORD;

    return ~0ULL << index_in_word;
}

static uint64_t last_word_mask_(uint64_t nbits) {
    uint64_t last_bits_num = nbits % ST_BITMAP_BITS_PER_WORD;

    if (last_bits_num != 0) {
        return (1ULL << last_bits_num) - 1;
    } else {
        return ~0ULL;
    }
}

int st_bitmap_are_all_cleared(uint64_t *bitmap, uint64_t nbits)
{
    int i;

    st_must(bitmap != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (bitmap[i] != 0) {
            return 0;
        }
    }

    if (nbits % ST_BITMAP_BITS_PER_WORD != 0) {
        if ((bitmap[i] & last_word_mask_(nbits)) != 0) {
            return 0;
        }
    }

    return 1;
}

int st_bitmap_are_all_set(uint64_t *bitmap, uint64_t nbits)
{
    int i;

    st_must(bitmap != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (~bitmap[i] != 0) {
            return 0;
        }
    }

    if (nbits % ST_BITMAP_BITS_PER_WORD != 0) {
        if ((~bitmap[i] & last_word_mask_(nbits)) != 0) {
            return 0;
        }
    }

    return 1;
}

int st_bitmap_equal(uint64_t *bitmap1, uint64_t *bitmap2, uint64_t nbits)
{
    int i;

    st_must(bitmap1 != NULL, ST_ARG_INVALID);
    st_must(bitmap2 != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (bitmap1[i] != bitmap2[i]) {
            return 0;
        }
    }

    if (nbits % ST_BITMAP_BITS_PER_WORD != 0) {
        if ((bitmap1[i] ^ bitmap2[i]) & last_word_mask_(nbits)) {
            return 0;
        }
    }

    return 1;
}

int st_bitmap_find_next_bit(uint64_t *bitmap, uint64_t nbits,
        uint64_t start_index, int bit_value)
{
    uint64_t word;
    int bit_i = -1;

    st_must(bitmap != NULL, ST_ARG_INVALID);
    st_must(start_index < nbits, ST_INDEX_OUT_OF_RANGE);
    st_must(bit_value == 1 || bit_value == 0, ST_ARG_INVALID);

    int start_word_i = start_index / ST_BITMAP_BITS_PER_WORD;

    int end_word_i = nbits / ST_BITMAP_BITS_PER_WORD;
    if (nbits % ST_BITMAP_BITS_PER_WORD != 0) {
        end_word_i += 1;
    }

    for (int word_i = start_word_i; word_i < end_word_i; word_i++) {

        word = bitmap[word_i];

        if (bit_value == 0) {
            //if find 0 bit, reverse the word, so can use find 1 bit flow
            word = ~word;
        }

        if (word_i == start_word_i) {
            word = word & first_word_mask_(start_index);
        }

        if (word_i == end_word_i - 1) {
            word = word & last_word_mask_(nbits);
        }

        if (word == 0) {
            continue;
        }

        for (int i = 0; i < ST_BITMAP_BITS_PER_WORD; i++) {
            bit_i = word_i * ST_BITMAP_BITS_PER_WORD + i;

            if (word & bit_mask_(bit_i)) {
                return bit_i;
            }
        }
    }

    return -1;
}
