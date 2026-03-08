#include "art.h"
#include "util.h"

/*
 * "BOREK" in a 5x7 pixel font. Each letter is 5 pixels wide, 7 tall.
 * Each byte stores one row, MSB-aligned (bit 4 = leftmost pixel).
 */

#define LETTER_W 5
#define LETTER_H 7
#define NUM_LETTERS 5

/* B */ static const uint8_t letter_B[LETTER_H] = {
    0x1E, /* 11110 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x1E, /* 11110 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x1E, /* 11110 */
};

/* O */ static const uint8_t letter_O[LETTER_H] = {
    0x0E, /* 01110 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x0E, /* 01110 */
};

/* R */ static const uint8_t letter_R[LETTER_H] = {
    0x1E, /* 11110 */
    0x11, /* 10001 */
    0x11, /* 10001 */
    0x1E, /* 11110 */
    0x14, /* 10100 */
    0x12, /* 10010 */
    0x11, /* 10001 */
};

/* E */ static const uint8_t letter_E[LETTER_H] = {
    0x1F, /* 11111 */
    0x10, /* 10000 */
    0x10, /* 10000 */
    0x1E, /* 11110 */
    0x10, /* 10000 */
    0x10, /* 10000 */
    0x1F, /* 11111 */
};

/* K */ static const uint8_t letter_K[LETTER_H] = {
    0x11, /* 10001 */
    0x12, /* 10010 */
    0x14, /* 10100 */
    0x18, /* 11000 */
    0x14, /* 10100 */
    0x12, /* 10010 */
    0x11, /* 10001 */
};

static const uint8_t *letters[NUM_LETTERS] = {
    letter_B, letter_O, letter_R, letter_E, letter_K
};

void draw_borek_art(lv_obj_t *canvas, int32_t x, int32_t y, int32_t pixel_size,
                    lv_color_t color) {
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, color);

    int32_t letter_spacing = pixel_size; /* 1 font-pixel gap between letters */
    int32_t cursor_x = x;

    for (int l = 0; l < NUM_LETTERS; l++) {
        const uint8_t *letter = letters[l];
        for (int row = 0; row < LETTER_H; row++) {
            uint8_t bits = letter[row];
            for (int col = 0; col < LETTER_W; col++) {
                if (bits & (1 << (LETTER_W - 1 - col))) {
                    canvas_draw_rect(canvas,
                                     cursor_x + col * pixel_size,
                                     y + row * pixel_size,
                                     pixel_size, pixel_size,
                                     &rect_dsc);
                }
            }
        }
        cursor_x += LETTER_W * pixel_size + letter_spacing;
    }
}
