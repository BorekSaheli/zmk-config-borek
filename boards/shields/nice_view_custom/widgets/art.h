#pragma once

#include <lvgl.h>
#include <stdint.h>

/* Draw one of the baked-in images (from widgets/art/) onto a 68x68 canvas.
 * idx is taken modulo the image count, so callers never need to bounds-check. */
void draw_custom_art(lv_obj_t *canvas, lv_color_t color, uint8_t idx);
