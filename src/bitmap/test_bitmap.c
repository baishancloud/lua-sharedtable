#include "bitmap.h"
#include "unittest/unittest.h"

st_test(bitmap, get_bit) {

    struct case_s {
        uint64_t bitmap[2];
        uint32_t set_index;
    } cases[] = {
        {{0x0000000000000001, 0x0000000000000000}, 0},
        {{0x0000000000000010, 0x0000000000000000}, 4},
        {{0x0000000010000000, 0x0000000000000000}, 28},
        {{0x0000000080000000, 0x0000000000000000}, 31},
        {{0x1000000000000000, 0x0000000000000000}, 60},
        {{0x0000000000000000, 0x0000000000000001}, 64},
        {{0x0000000000000000, 0x8000000000000000}, 127},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        for (int j = 0; j < 2 * sizeof(uint64_t) * 8; j ++) {
            if (j == c.set_index) {
                st_ut_eq(1, st_bitmap_get(c.bitmap, j), "bit has set");
            } else {
                st_ut_eq(0, st_bitmap_get(c.bitmap, j), "bit not set");
            }
        }
    }
}

st_test(bitmap, set_bit) {

    uint64_t bitmap[2] = {0};
    uint32_t set_indexes[] = {0, 1, 5, 9, 10, 11, 29, 31, 33, 61, 62, 63, 64, 65, 72, 127};

    int set_i;

    for (int i = 0; i < st_nelts(set_indexes); i++) {
        set_i = set_indexes[i];
        st_bitmap_set(bitmap, set_i);

        // check all set bit from begin
        for (int j = 0; j < 2 * sizeof(uint64_t) * 8; j ++) {

            int bit_set = 0;

            for (int k = 0; k <= i; k++) {
                if (j == set_indexes[k]) {
                    bit_set = 1;
                    break;
                }
            }

            if (bit_set == 1) {
                st_ut_eq(1, st_bitmap_get(bitmap, j), "bit has set");
            } else {
                st_ut_eq(0, st_bitmap_get(bitmap, j), "bit not set");
            }
        }
    }
}

st_test(bitmap, clear_bit) {

    uint64_t bitmap[2] = {0xffffffffffffffff, 0xffffffffffffffff};
    uint32_t clear_indexes[] = {0, 1, 5, 9, 10, 11, 29, 31, 33, 61, 62, 63, 64, 65, 72, 127};

    int clear_i;

    for (int i = 0; i < st_nelts(clear_indexes); i++) {
        clear_i = clear_indexes[i];
        st_bitmap_clear(bitmap, clear_i);

        // check all clear bit from begin
        for (int j = 0; j < 2 * sizeof(uint64_t) * 8; j ++) {

            int bit_clear = 0;

            for (int k = 0; k <= i; k++) {
                if (j == clear_indexes[k]) {
                    bit_clear = 1;
                    break;
                }
            }

            if (bit_clear == 1) {
                st_ut_eq(0, st_bitmap_get(bitmap, j), "bit has cleared");
            } else {
                st_ut_eq(1, st_bitmap_get(bitmap, j), "bit not cleared");
            }
        }
    }
}

st_test(bitmap, all_cleared) {

    struct case_s {
        uint64_t bitmap[2];
        uint32_t nbits;
        int is_cleared;
    } cases[] = {
        {{0x0000000000000000, 0x0000000000000000}, 1, 1},
        {{0x0000000000000000, 0x0000000000000000}, 64, 1},
        {{0x0000000000000000, 0x0000000000000000}, 128, 1},

        {{0x0000000000000010, 0x0000000000000000}, 4, 1},
        {{0x0000000000000010, 0x0000000000000000}, 5, 0},
        {{0x0000000000000010, 0x0000000000000000}, 128, 0},

        {{0x0000000000000000, 0x0000000000000001}, 64, 1},
        {{0x0000000000000000, 0x0000000000000001}, 65, 0},
        {{0x0000000000000000, 0x0000000000000001}, 128, 0},

    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.is_cleared, st_bitmap_are_all_cleared(c.bitmap, c.nbits), "set result is right");
    }

    st_ut_eq(ST_ARG_INVALID, st_bitmap_are_all_cleared(NULL, 2), "bitmap is NULL");
}

st_test(bitmap, all_set) {

    struct case_s {
        uint64_t bitmap[2];
        uint32_t nbits;
        int is_set;
    } cases[] = {
        {{0xffffffffffffffff, 0xffffffffffffffff}, 1, 1},
        {{0xffffffffffffffff, 0xffffffffffffffff}, 64, 1},
        {{0xffffffffffffffff, 0xffffffffffffffff}, 128, 1},

        {{0xffffffffffffff01, 0xffffffffffffffff}, 1, 1},
        {{0xffffffffffffff01, 0xffffffffffffffff}, 2, 0},
        {{0xffffffffffffff01, 0xffffffffffffffff}, 128, 0},

        {{0xffffffffffffffff, 0xfffffffffffffff0}, 64, 1},
        {{0xffffffffffffffff, 0xfffffffffffffff0}, 65, 0},
        {{0xffffffffffffffff, 0xfffffffffffffff0}, 128, 0},

    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.is_set, st_bitmap_are_all_set(c.bitmap, c.nbits), "clear result is right");
    }

    st_ut_eq(ST_ARG_INVALID, st_bitmap_are_all_set(NULL, 2), "bitmap is NULL");
}

st_test(bitmap, equall) {

    struct case_s {
        uint64_t bitmap1[2];
        uint64_t bitmap2[2];
        uint32_t nbits;
        int is_equal;
    } cases[] = {
        {{0xffffffffffffffff, 0xffffffffffffffff},
         {0xffffffffffffffff, 0xffffffffffffffff},
         128, 1},

        {{0x0000000000000000, 0x0000000000000000},
         {0x0000000000000000, 0x0000000000000000},
         128, 1},

        {{0x0001000000010000, 0x0001000000010000},
         {0x0001000000010000, 0x0001000000010000},
         64, 1},

        {{0x0000000000000000, 0x0000000000000000},
         {0x1111111111111111, 0x1111111111111111},
         0, 1},

        {{0x0000000000000000, 0x0000000000000000},
         {0x1111111111111111, 0x1111111111111111},
         128, 0},

        {{0x0000000000000000, 0x0000000000000000},
         {0x0000000000000000, 0x0000000000000001},
         64, 1},

        {{0x0000000000000000, 0x0000000000000000},
         {0x0000000000000000, 0x0000000000000001},
         65, 0},

        {{0x0000000000000000, 0x0000000000000000},
         {0x0000000000000000, 0x0000000000000001},
         128, 0},

    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.is_equal, st_bitmap_equal(c.bitmap1, c.bitmap2, c.nbits), "bits equal is right");
    }

    uint64_t bitmap;

    st_ut_eq(ST_ARG_INVALID, st_bitmap_equal(NULL, &bitmap, 2), "first bitmap is NULL");
    st_ut_eq(ST_ARG_INVALID, st_bitmap_equal(&bitmap, NULL, 2), "first bitmap is NULL");
}

st_test(bitmap, find_next_bit) {

    uint64_t set_bitmap[3] = {0x0000000001010001, 0x0000000000000000, 0x0000000000000fff};
    uint64_t clear_bitmap[3] = {0xfffffffffefefffe, 0xffffffffffffffff, 0xfffffffffffff000};

    struct case_s {
        uint32_t start_idx;
        uint32_t end_idx;
        uint32_t find_idx;
    } cases[] = {
        {0, 1, 0},
        {0, 2, 0},
        {1, 2, -1},

        {3, 17, 16},
        {16, 17, 16},
        {3, 65, 16},
        {16, 129, 16},

        {15, 16, -1},
        {17, 18, -1},

        {63, 129, 128},
        {64, 130, 128},
        {65, 192, 128},

        {63, 64, -1},
        {63, 65, -1},
        {63, 128, -1},
        {64, 128, -1},
        {65, 128, -1},
        {65, 128, -1},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.find_idx, st_bitmap_find_set_bit(set_bitmap, c.start_idx, c.end_idx), "find set bit is right");

        st_ut_eq(c.find_idx, st_bitmap_find_clear_bit(clear_bitmap, c.start_idx, c.end_idx), "find clear bit is right");
    }

    uint64_t bitmap;

    st_ut_eq(ST_ARG_INVALID, st_bitmap_find_next_bit(NULL, 1, 2, 0), "bitmap is NULL");

    st_ut_eq(ST_INDEX_OUT_OF_RANGE, st_bitmap_find_next_bit(&bitmap, 2, 2, 0), "bitmap start_index is out of range");

    st_ut_eq(ST_INDEX_OUT_OF_RANGE, st_bitmap_find_next_bit(&bitmap, 3, 2, 0), "bitmap start_index is out of range");

    st_ut_eq(ST_ARG_INVALID, st_bitmap_find_next_bit(&bitmap, 1, 2, 2), "bitmap start_index is out of range");
}

st_ut_main;
