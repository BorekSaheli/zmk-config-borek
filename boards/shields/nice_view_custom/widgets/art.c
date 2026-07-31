#include "art.h"
#include "util.h"
#include "art/art_list.h"

void draw_custom_art(lv_obj_t *canvas, lv_color_t color, uint8_t idx) {
    const uint8_t *pixels = art_images[idx % ART_IMAGES_COUNT];

    /* lv_canvas_set_px writes the buffer directly — much cheaper than
     * spinning up a draw layer per pixel, which kept the display work
     * queue busy for the whole redraw. */
    for (int row = 0; row < ART_IMG_H; row++) {
        const uint8_t *row_data = pixels + row * ART_IMG_ROW_BYTES;
        for (int col = 0; col < ART_IMG_W; col++) {
            int byte_idx = col / 8;
            int bit_idx = 7 - (col % 8);
            if (row_data[byte_idx] & (1 << bit_idx)) {
                lv_canvas_set_px(canvas, col, row, color, LV_OPA_COVER);
            }
        }
    }
}
