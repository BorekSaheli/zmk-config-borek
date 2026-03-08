#include <zephyr/kernel.h>
#include "util.h"
#include <string.h>

void rotate_canvas(lv_obj_t *canvas) {
    static uint8_t cbuf_tmp[CANVAS_BUF_SIZE];
    uint8_t *cbuf = lv_canvas_get_draw_buf(canvas)->data;
    memcpy(cbuf_tmp, cbuf, CANVAS_BUF_SIZE);

    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    const int32_t stride = lv_draw_buf_width_to_stride(CANVAS_SIZE, CANVAS_COLOR_FORMAT);
    lv_draw_sw_rotate(cbuf_tmp, cbuf, CANVAS_SIZE, CANVAS_SIZE, stride, stride,
                      LV_DISPLAY_ROTATION_270, CANVAS_COLOR_FORMAT);
}

void fill_background(lv_obj_t *canvas) {
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align) {
    lv_draw_label_dsc_init(label_dsc);
    label_dsc->color = color;
    label_dsc->font = font;
    label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
    lv_draw_rect_dsc_init(rect_dsc);
    rect_dsc->bg_color = bg_color;
    rect_dsc->bg_opa = LV_OPA_COVER;
}

void canvas_draw_rect(lv_obj_t *canvas, int32_t x, int32_t y, int32_t w, int32_t h,
                      lv_draw_rect_dsc_t *rect_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_area_t coords = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(&layer, rect_dsc, &coords);
    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_text(lv_obj_t *canvas, int32_t x, int32_t y, int32_t max_w,
                      lv_draw_label_dsc_t *label_dsc, const char *text) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    label_dsc->text = text;
    lv_area_t coords = {x, y, x + max_w - 1, y + 20};
    lv_draw_label(&layer, label_dsc, &coords);
    lv_canvas_finish_layer(canvas, &layer);
}
