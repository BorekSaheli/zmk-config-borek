#pragma once

#include <lvgl.h>

/* Draw the "BOREK" pixel art onto a canvas at the given position.
 * pixel_size controls the scale (each font pixel = pixel_size x pixel_size). */
void draw_borek_art(lv_obj_t *canvas, int32_t x, int32_t y, int32_t pixel_size,
                    lv_color_t color);
