#include "bitmap.h"
#include "unittest/unittest.h"

st_test(bitmap, get_bit) {

    struct case_s {
        uint32_t bitmap[2];
        int set_index;
    } cases[] = {
        {{0x00000001, 0x00000000}, 0},
        {{0x00000010, 0x00000000}, 4},
        {{0x00000100, 0x00000000}, 8},
        {{0x10000000, 0x00000000}, 28},
        {{0x80000000, 0x00000000}, 31},
        {{0x00000000, 0x00000001}, 32},
        {{0x00000000, 0x10000000}, 60},
        {{0x00000000, 0x80000000}, 63},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        for (int j = 0; j < 2 * sizeof(uint32_t) * 8; j ++) {
            if (j == c.set_index) {
                st_ut_eq(1, st_bitmap_get_bit(c.bitmap, j), "bit has set");
            } else {
                st_ut_eq(0, st_bitmap_get_bit(c.bitmap, j), "bit not set");
            }
        }
    }
}

st_test(bitmap, set_bit) {

    uint32_t bitmap[2] = {0};
    int set_indexes[] = {0, 1, 5, 9, 10, 11, 29, 31, 33, 61, 62, 63};

    int set_i;

    for (int i = 0; i < st_nelts(set_indexes); i++) {
        set_i = set_indexes[i];
        st_bitmap_set_bit(bitmap, set_i);

        // check all set bit from begin
        for (int j = 0; j < 2 * sizeof(uint32_t) * 8; j ++) {

            int bit_set = 0;

            for (int k = 0; k <= i; k++) {
                if (j == set_indexes[k]) {
                    bit_set = 1;
                    break;
                }
            }

            if (bit_set == 1) {
                st_ut_eq(1, st_bitmap_get_bit(bitmap, j), "bit has set");
            } else {
                st_ut_eq(0, st_bitmap_get_bit(bitmap, j), "bit not set");
            }
        }
    }
}

st_test(bitmap, clear_bit) {

    uint32_t bitmap[2] = {0xffffffff, 0xffffffff};
    int clear_indexes[] = {0, 1, 5, 9, 10, 11, 29, 31, 33, 61, 62, 63};

    int clear_i;

    for (int i = 0; i < st_nelts(clear_indexes); i++) {
        clear_i = clear_indexes[i];
        st_bitmap_clear_bit(bitmap, clear_i);

        // check all clear bit from begin
        for (int j = 0; j < 2 * sizeof(uint32_t) * 8; j ++) {

            int bit_clear = 0;

            for (int k = 0; k <= i; k++) {
                if (j == clear_indexes[k]) {
                    bit_clear = 1;
                    break;
                }
            }

            if (bit_clear == 1) {
                st_ut_eq(0, st_bitmap_get_bit(bitmap, j), "bit has cleared");
            } else {
                st_ut_eq(1, st_bitmap_get_bit(bitmap, j), "bit not cleared");
            }
        }
    }
}

st_test(bitmap, all_cleared) {

    struct case_s {
        uint32_t bitmap[2];
        int nbits;
        int is_cleared;
    } cases[] = {
        {{0x00000000, 0x00000000}, 1, 1},
        {{0x00000000, 0x00000000}, 32, 1},
        {{0x00000000, 0x00000000}, 64, 1},

        {{0x00000010, 0x00000000}, 4, 1},
        {{0x00000010, 0x00000000}, 5, 0},
        {{0x00000010, 0x00000000}, 64, 0},

        {{0x00000000, 0x00000001}, 32, 1},
        {{0x00000000, 0x00000001}, 33, 0},
        {{0x00000000, 0x00000001}, 64, 0},

    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.is_cleared, st_bitmap_are_all_cleared(c.bitmap, c.nbits), "set result is right");
    }

    st_ut_eq(ST_ARG_INVALID, st_bitmap_are_all_cleared(NULL, 2), "bitmap is NULL");
}

st_test(bitmap, all_set) {

    struct case_s {
        uint32_t bitmap[2];
        int nbits;
        int is_set;
    } cases[] = {
        {{0xffffffff, 0xffffffff}, 1, 1},
        {{0xffffffff, 0xffffffff}, 32, 1},
        {{0xffffffff, 0xffffffff}, 64, 1},

        {{0xffffff01, 0xffffffff}, 1, 1},
        {{0xffffff01, 0xffffffff}, 2, 0},
        {{0xffffff01, 0xffffffff}, 64, 0},

        {{0xffffffff, 0xfffffff0}, 32, 1},
        {{0xffffffff, 0xfffffff0}, 33, 0},
        {{0xffffffff, 0xfffffff0}, 64, 0},

    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.is_set, st_bitmap_are_all_set(c.bitmap, c.nbits), "clear result is right");
    }

    st_ut_eq(ST_ARG_INVALID, st_bitmap_are_all_set(NULL, 2), "bitmap is NULL");
}

st_test(bitmap, equall) {

    struct case_s {
        uint32_t bitmap1[2];
        uint32_t bitmap2[2];
        int nbits;
        int is_equal;
    } cases[] = {
        {{0xffffffff, 0xffffffff},
         {0xffffffff, 0xffffffff},
         64, 1},

        {{0x00000000, 0x00000000},
         {0x00000000, 0x00000000},
         64, 1},

        {{0x00010000, 0x00010000},
         {0x00010000, 0x00010000},
         64, 1},

        {{0x00000000, 0x00000000},
         {0x11111111, 0x11111111},
         0, 1},

        {{0x00000000, 0x00000000},
         {0x11111111, 0x11111111},
         64, 0},

        {{0x00000000, 0x00000000},
         {0x00000000, 0x00000001},
         32, 1},

        {{0x00000000, 0x00000000},
         {0x00000000, 0x00000001},
         33, 0},

        {{0x00000000, 0x00000000},
         {0x00000000, 0x00000001},
         64, 0},

    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.is_equal, st_bitmap_equal(c.bitmap1, c.bitmap2, c.nbits), "bits equal is right");
    }

    uint32_t bitmap;

    st_ut_eq(ST_ARG_INVALID, st_bitmap_equal(NULL, &bitmap, 2), "first bitmap is NULL");
    st_ut_eq(ST_ARG_INVALID, st_bitmap_equal(&bitmap, NULL, 2), "first bitmap is NULL");
}

st_test(bitmap, find_next_bit) {

    uint32_t set_bitmap[3]   = {0x01010001, 0x00000000, 0x00000fff};
    uint32_t clear_bitmap[3] = {0xfefefffe, 0xffffffff, 0xfffff000};

    struct case_s {
        uint32_t start_index;
        int nbits;
        int find_index;
    } cases[] = {
        {0, 1, 0},
        {0, 2, 0},
        {1, 2, -1},

        {3, 17, 16},
        {16, 17, 16},
        {3, 33, 16},
        {16, 65, 16},

        {15, 16, -1},
        {17, 18, -1},

        {31, 65, 64},
        {32, 66, 64},
        {33, 96, 64},

        {31, 32, -1},
        {31, 33, -1},
        {31, 64, -1},
        {32, 64, -1},
        {33, 64, -1},
        {33, 64, -1},
    };

    for (int i = 0; i < st_nelts(cases); i++) {
        st_typeof(cases[0]) c = cases[i];

        st_ut_eq(c.find_index, st_bitmap_find_set_bit(set_bitmap, c.nbits, c.start_index), "find set bit is right");

        st_ut_eq(c.find_index, st_bitmap_find_clear_bit(clear_bitmap, c.nbits, c.start_index), "find clear bit is right");
    }

    uint32_t bitmap;

    st_ut_eq(ST_ARG_INVALID, st_bitmap_find_next_bit(NULL, 2, 1, 0), "bitmap is NULL");

    st_ut_eq(ST_INDEX_OUT_OF_RANGE, st_bitmap_find_next_bit(&bitmap, 2, 2, 0), "bitmap start_index is out of range");

    st_ut_eq(ST_INDEX_OUT_OF_RANGE, st_bitmap_find_next_bit(&bitmap, 2, 3, 0), "bitmap start_index is out of range");

    st_ut_eq(ST_ARG_INVALID, st_bitmap_find_next_bit(&bitmap, 2, 1, 2), "bitmap start_index is out of range");
}

st_ut_main;
