#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>


#include "art.h"
#include "screen.h"


static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ── drawing helpers ────────────────────────────────────── */

/* Top canvas (right side of display): battery + BT info */
static void draw_top(lv_obj_t *canvas, const struct status_state *state) {
  fill_background(canvas);

  lv_draw_label_dsc_t label_dsc;
  init_label_dsc(&label_dsc, LVGL_FOREGROUND, lv_font_default(),
                 LV_TEXT_ALIGN_CENTER);

  /* Battery percentage */
  char bat[8];
  snprintf(bat, sizeof(bat), "%d%%", state->battery);
  canvas_draw_text(canvas, 0, 2, 68, &label_dsc, bat);

  /* BT profile */
  char bt[12];
  if (state->active_profile_connected) {
    snprintf(bt, sizeof(bt), "BT%d *", state->active_profile_index + 1);
  } else {
    snprintf(bt, sizeof(bt), "BT%d", state->active_profile_index + 1);
  }
  canvas_draw_text(canvas, 0, 18, 68, &label_dsc, bt);

  rotate_canvas(canvas);
}

/* Middle canvas (center): BOREK pixel art */
static void draw_middle(lv_obj_t *canvas) {
  fill_background(canvas);

  /* BOREK at 2x scale: 5*5*2 + 4*2 = 58 wide, 7*2 = 14 tall */
  int32_t art_w = (5 * 5 * 2) + (4 * 2); /* 58 */
  int32_t art_h = 7 * 2;                 /* 14 */
  int32_t ax = (68 - art_w) / 2;         /* 5 */
  int32_t ay = (68 - art_h) / 2;         /* 27 */

  draw_borek_art(canvas, ax, ay, 2, LVGL_FOREGROUND);

  rotate_canvas(canvas);
}

/* Bottom canvas (left side of display): layer name */
static void draw_bottom(lv_obj_t *canvas, const struct status_state *state) {
  fill_background(canvas);

  lv_draw_label_dsc_t label_dsc;
  init_label_dsc(&label_dsc, LVGL_FOREGROUND, lv_font_default(),
                 LV_TEXT_ALIGN_CENTER);

  char text[16];
  if (state->layer_label) {
    snprintf(text, sizeof(text), "%s", state->layer_label);
  } else {
    snprintf(text, sizeof(text), "Layer %d", state->layer_index);
  }

  canvas_draw_text(canvas, 0, 24, 68, &label_dsc, text);

  rotate_canvas(canvas);
}

/* ── state extraction ───────────────────────────────────── */

static struct status_state get_state(const zmk_event_t *_eh) {
  return (struct status_state){
      .battery = zmk_battery_state_of_charge(),
      .selected_endpoint = zmk_endpoint_get_selected(),
      .active_profile_index = zmk_ble_active_profile_index(),
      .active_profile_connected = zmk_ble_active_profile_is_connected(),
      .active_profile_bonded = !zmk_ble_active_profile_is_open(),
      .layer_index = zmk_keymap_highest_layer_active(),
      .layer_label = zmk_keymap_layer_name(zmk_keymap_highest_layer_active()),
  };
}

/* ── update callbacks ───────────────────────────────────── */

static void update_top(struct status_state state) {
  struct zmk_widget_screen *widget;
  SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
    widget->state = state;
    lv_obj_t *top = lv_obj_get_child(widget->obj, 0);
    draw_top(top, &widget->state);
  }
}

static void update_bottom(struct status_state state) {
  struct zmk_widget_screen *widget;
  SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
    widget->state = state;
    lv_obj_t *bottom = lv_obj_get_child(widget->obj, 2);
    draw_bottom(bottom, &widget->state);
  }
}

static void full_update(struct status_state state) {
  struct zmk_widget_screen *widget;
  SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
    widget->state = state;

    lv_obj_t *top = lv_obj_get_child(widget->obj, 0);
    draw_top(top, &widget->state);

    lv_obj_t *middle = lv_obj_get_child(widget->obj, 1);
    draw_middle(middle);

    lv_obj_t *bottom = lv_obj_get_child(widget->obj, 2);
    draw_bottom(bottom, &widget->state);
  }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery, struct status_state, update_top,
                            get_state)
ZMK_SUBSCRIPTION(widget_battery, zmk_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer, struct status_state, update_bottom,
                            get_state)
ZMK_SUBSCRIPTION(widget_layer, zmk_layer_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_bt, struct status_state, full_update,
                            get_state)
ZMK_SUBSCRIPTION(widget_bt, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_bt, zmk_endpoint_changed);

/* ── init ───────────────────────────────────────────────── */

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
  /* Container object spanning the full display */
  widget->obj = lv_obj_create(parent);
  lv_obj_set_size(widget->obj, 160, 68);
  lv_obj_set_style_bg_color(widget->obj, LVGL_BACKGROUND, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);

  /* Canvas 0 — top (rightmost on display) */
  lv_obj_t *top = lv_canvas_create(widget->obj);
  lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE,
                       CANVAS_COLOR_FORMAT);

  /* Canvas 1 — middle */
  lv_obj_t *middle = lv_canvas_create(widget->obj);
  lv_obj_align(middle, LV_ALIGN_TOP_RIGHT, BUFFER_OFFSET_MIDDLE, 0);
  lv_canvas_set_buffer(middle, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE,
                       CANVAS_COLOR_FORMAT);

  /* Canvas 2 — bottom (leftmost on display) */
  lv_obj_t *bottom = lv_canvas_create(widget->obj);
  lv_obj_align(bottom, LV_ALIGN_TOP_RIGHT, BUFFER_OFFSET_BOTTOM, 0);
  lv_canvas_set_buffer(bottom, widget->cbuf3, CANVAS_SIZE, CANVAS_SIZE,
                       CANVAS_COLOR_FORMAT);

  sys_slist_append(&widgets, &widget->node);

  /* Initial draw */
  widget->state = get_state(NULL);
  draw_top(top, &widget->state);
  draw_middle(middle);
  draw_bottom(bottom, &widget->state);

  widget_battery_init();
  widget_layer_init();
  widget_bt_init();

  return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) {
  return widget->obj;
}
