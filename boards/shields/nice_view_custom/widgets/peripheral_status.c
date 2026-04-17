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
#include "art/art_list.h"
#include "../src/image_sync.h"

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

/* Middle canvas: custom pixel art — which image is driven by image_sync */
static void draw_art(lv_obj_t *canvas, uint8_t idx) {
    fill_background(canvas);

    draw_custom_art(canvas, LVGL_FOREGROUND, idx);

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

static uint8_t current_art_idx;
static uint8_t pending_art_idx;
static struct k_work art_update_work;

static void art_update_work_cb(struct k_work *work) {
    current_art_idx = pending_art_idx;
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_obj_t *art = lv_obj_get_child(widget->obj, 1);
        draw_art(art, current_art_idx);
    }
}

static void on_image_sync(uint8_t idx) {
    pending_art_idx = idx;
    k_work_submit(&art_update_work);
}

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

    k_work_init(&art_update_work, art_update_work_cb);
    current_art_idx = image_sync_get_index();
    draw_art(art, current_art_idx);
    image_sync_register_listener(on_image_sync);

    widget_peripheral_init();

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) {
    return widget->obj;
}
