#include "bitmap.h"

#define ST_BITMAP_BITS_PER_WORD (sizeof(uint32_t) * 8)

static uint32_t _bit_mask(uint32_t bit_index) {
    uint32_t index_in_word = bit_index % ST_BITMAP_BITS_PER_WORD;

    return 1U << index_in_word;
}

uint32_t st_bitmap_get_bit(uint32_t *bitmap, uint32_t bit_index) {
    uint32_t word_index = bit_index / ST_BITMAP_BITS_PER_WORD;

    return bitmap[word_index] & _bit_mask(bit_index) ? 1 : 0;
}

void st_bitmap_set_bit(uint32_t *bitmap, uint32_t bit_index) {
    uint32_t word_index = bit_index / ST_BITMAP_BITS_PER_WORD;

    bitmap[word_index] |= _bit_mask(bit_index);
}

void st_bitmap_clear_bit(uint32_t *bitmap, uint32_t bit_index) {
    uint32_t word_index = bit_index / ST_BITMAP_BITS_PER_WORD;

    bitmap[word_index] &= ~_bit_mask(bit_index);
}

static uint32_t _first_word_mask(uint32_t start_index) {
    uint32_t index_in_word = start_index % ST_BITMAP_BITS_PER_WORD;

    return ~0U << index_in_word;
}

static uint32_t _last_word_mask(uint32_t nbits) {
    uint32_t last_bits_num = nbits % ST_BITMAP_BITS_PER_WORD;

    if (last_bits_num != 0) {
        return (1U << last_bits_num) - 1;
    } else {
        return ~0U;
    }
}

int st_bitmap_are_all_cleared(uint32_t *bitmap, uint32_t nbits)
{
    int i;

    st_must(bitmap != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (bitmap[i] != 0) {
            return 0;
        }
    }

    if (nbits % ST_BITMAP_BITS_PER_WORD != 0) {
        if ((bitmap[i] & _last_word_mask(nbits)) != 0) {
            return 0;
        }
    }

    return 1;
}

int st_bitmap_are_all_set(uint32_t *bitmap, uint32_t nbits)
{
    int i;

    st_must(bitmap != NULL, ST_ARG_INVALID);

    for (i = 0; i < nbits / ST_BITMAP_BITS_PER_WORD; i++) {
        if (~bitmap[i] != 0) {
            return 0;
        }
    }

    if (nbits % ST_BITMAP_BITS_PER_WORD != 0) {
        if ((~bitmap[i] & _last_word_mask(nbits)) != 0) {
            return 0;
        }
    }

    return 1;
}

int st_bitmap_equal(uint32_t *bitmap1, uint32_t *bitmap2, uint32_t nbits)
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
        if ((bitmap1[i] ^ bitmap2[i]) & _last_word_mask(nbits)) {
            return 0;
        }
    }

    return 1;
}

int st_bitmap_find_next_bit(uint32_t *bitmap, uint32_t nbits,
        uint32_t start_index, int bit_value)
{
    uint32_t word;
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
            word = word & _first_word_mask(start_index);
        }

        if (word_i == end_word_i - 1) {
            word = word & _last_word_mask(nbits);
        }

        if (word == 0) {
            continue;
        }

        for (int i = 0; i < ST_BITMAP_BITS_PER_WORD; i++) {
            bit_i = word_i * ST_BITMAP_BITS_PER_WORD + i;

            if (word & _bit_mask(bit_i)) {
                return bit_i;
            }
        }
    }

    return -1;
}
