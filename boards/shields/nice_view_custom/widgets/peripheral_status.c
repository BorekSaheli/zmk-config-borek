#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include "peripheral_status.h"
#include "art.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Top canvas (right side): battery + connection status */
static void draw_top(lv_obj_t *canvas, const struct status_state *state) {
    fill_background(canvas);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, lv_font_default(), LV_TEXT_ALIGN_CENTER);

    /* Battery */
    char bat[8];
    snprintf(bat, sizeof(bat), "%d%%", state->battery);
    canvas_draw_text(canvas, 0, 2, 68, &label_dsc, bat);

    /* Connection status */
    canvas_draw_text(canvas, 0, 18, 68, &label_dsc,
                     state->connected ? "LINK" : "----");

    rotate_canvas(canvas);
}

/* Middle canvas: BOREK pixel art */
static void draw_art(lv_obj_t *canvas) {
    fill_background(canvas);

    int32_t art_w = (5 * 5 * 2) + (4 * 2); /* 58 */
    int32_t art_h = 7 * 2;                   /* 14 */
    int32_t ax = (68 - art_w) / 2;
    int32_t ay = (68 - art_h) / 2;

    draw_borek_art(canvas, ax, ay, 2, LVGL_FOREGROUND);

    rotate_canvas(canvas);
}

static struct status_state get_state(const zmk_event_t *_eh) {
    return (struct status_state){
        .battery = zmk_battery_state_of_charge(),
        .connected = zmk_split_bt_peripheral_is_connected(),
    };
}

static void update_cb(struct status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state = state;
        lv_obj_t *top = lv_obj_get_child(widget->obj, 0);
        draw_top(top, &widget->state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral, struct status_state, update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_peripheral, zmk_split_peripheral_status_changed);

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    /* Container */
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);
    lv_obj_set_style_bg_color(widget->obj, LVGL_BACKGROUND, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);

    /* Canvas 0 — top (rightmost): status */
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    /* Canvas 1 — art */
    lv_obj_t *art = lv_canvas_create(widget->obj);
    lv_obj_align(art, LV_ALIGN_TOP_RIGHT, BUFFER_OFFSET_MIDDLE, 0);
    lv_canvas_set_buffer(art, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    sys_slist_append(&widgets, &widget->node);

    widget->state = get_state(NULL);
    draw_top(top, &widget->state);
    draw_art(art);

    widget_peripheral_init();

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) {
    return widget->obj;
}
