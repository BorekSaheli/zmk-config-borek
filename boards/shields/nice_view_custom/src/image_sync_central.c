/*
 * Central side: owns the hourly rotation counter, pushes the peripheral's
 * image index over one GATT characteristic, and mirrors the current
 * keymap layer label over a second one. Peripheral reads both and updates
 * its own display accordingly — needed because ZMK's keymap / layer_state
 * event APIs are compiled only on central.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "image_sync.h"
#include "../widgets/art/art_list.h"

LOG_MODULE_REGISTER(image_sync_central, CONFIG_ZMK_LOG_LEVEL);

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(IMAGE_SYNC_SERVICE_UUID);
static struct bt_uuid_128 img_chr_uuid = BT_UUID_INIT_128(IMAGE_SYNC_CHAR_UUID);
static struct bt_uuid_128 layer_chr_uuid = BT_UUID_INIT_128(LAYER_SYNC_CHAR_UUID);

static struct bt_conn *peripheral_conn;
static uint16_t img_char_handle;
static uint16_t layer_char_handle;
static uint16_t svc_end_handle;
static struct bt_gatt_discover_params discover_params;

static uint32_t hour_counter;
static uint8_t local_idx;
static image_sync_listener_t img_listener_cb;

static char current_label[LAYER_SYNC_LABEL_MAX];

static struct k_timer hour_timer;
static struct k_work_delayable discover_work;
static struct k_work push_img_work;
static struct k_work push_layer_work;

static void push_peripheral_idx(void) {
    if (!peripheral_conn || !img_char_handle) {
        return;
    }
    uint8_t idx = (hour_counter + 1) % ART_IMAGES_COUNT;
    int err = bt_gatt_write_without_response(peripheral_conn, img_char_handle, &idx, 1, false);
    if (err) {
        LOG_WRN("image_sync idx write failed: %d", err);
    }
}

static void push_peripheral_layer(void) {
    if (!peripheral_conn || !layer_char_handle) {
        return;
    }
    size_t len = strnlen(current_label, LAYER_SYNC_LABEL_MAX - 1) + 1;
    int err = bt_gatt_write_without_response(peripheral_conn, layer_char_handle, current_label,
                                             len, false);
    if (err) {
        LOG_WRN("image_sync layer write failed: %d", err);
    }
}

static void push_img_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    push_peripheral_idx();
}

static void push_layer_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    push_peripheral_layer();
}

static void update_local_label(void) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    const char *name = zmk_keymap_layer_name(layer);
    if (!name) {
        snprintk(current_label, sizeof(current_label), "Layer %u", layer);
    } else {
        strncpy(current_label, name, sizeof(current_label) - 1);
        current_label[sizeof(current_label) - 1] = '\0';
    }
}

static void update_local(void) {
    local_idx = hour_counter % ART_IMAGES_COUNT;
    if (img_listener_cb) {
        img_listener_cb(local_idx);
    }
}

uint8_t image_sync_get_index(void) { return local_idx; }

void image_sync_register_listener(image_sync_listener_t cb) {
    img_listener_cb = cb;
    if (cb) {
        cb(local_idx);
    }
}

/* On central, layer state is available via existing ZMK APIs — the sync
 * service's layer-listener API is peripheral-only. These stubs keep the
 * header surface identical across both sides. */
const char *layer_sync_get_label(void) { return current_label; }
void layer_sync_register_listener(layer_sync_listener_t cb) { ARG_UNUSED(cb); }

static void hour_tick(struct k_timer *t) {
    ARG_UNUSED(t);
    hour_counter++;
    update_local();
    k_work_submit(&push_img_work);
}

static int layer_event_cb(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    update_local_label();
    k_work_submit(&push_layer_work);
    return 0;
}

ZMK_LISTENER(image_sync_layer, layer_event_cb);
ZMK_SUBSCRIPTION(image_sync_layer, zmk_layer_state_changed);

static uint8_t layer_char_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("image_sync discovery done (img=%u, layer=%u)",
                img_char_handle, layer_char_handle);
        /* Push current state so the peripheral catches up. */
        k_work_submit(&push_img_work);
        k_work_submit(&push_layer_work);
        return BT_GATT_ITER_STOP;
    }
    layer_char_handle = bt_gatt_attr_value_handle(attr);
    return BT_GATT_ITER_STOP;
}

static uint8_t img_char_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                    struct bt_gatt_discover_params *params) {
    if (!attr) {
        return BT_GATT_ITER_STOP;
    }
    img_char_handle = bt_gatt_attr_value_handle(attr);
    /* Chain into the second characteristic discovery. */
    discover_params.uuid = &layer_chr_uuid.uuid;
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = svc_end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = layer_char_discover_cb;
    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_WRN("layer_sync char discover start failed: %d", err);
    }
    return BT_GATT_ITER_STOP;
}

static uint8_t svc_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("image_sync service not found on peripheral");
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_service_val *svc = attr->user_data;
    svc_end_handle = svc ? svc->end_handle : 0xffff;
    discover_params.uuid = &img_chr_uuid.uuid;
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = svc_end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = img_char_discover_cb;
    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_WRN("image_sync char discover start failed: %d", err);
    }
    return BT_GATT_ITER_STOP;
}

static void discover_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    if (!peripheral_conn) {
        return;
    }
    discover_params.uuid = &svc_uuid.uuid;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    discover_params.func = svc_discover_cb;
    int err = bt_gatt_discover(peripheral_conn, &discover_params);
    if (err) {
        LOG_WRN("image_sync svc discover start failed: %d", err);
    }
}

static void connected_cb(struct bt_conn *conn, uint8_t err) {
    if (err) {
        return;
    }
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) < 0) {
        return;
    }
    /* Only the outgoing connection to the split peripheral. */
    if (info.role != BT_CONN_ROLE_CENTRAL) {
        return;
    }
    if (peripheral_conn) {
        return;
    }
    peripheral_conn = bt_conn_ref(conn);
    img_char_handle = 0;
    layer_char_handle = 0;
    /* Let ZMK's split service finish its own discovery first. */
    k_work_schedule(&discover_work, K_SECONDS(3));
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason) {
    if (conn != peripheral_conn) {
        return;
    }
    bt_conn_unref(peripheral_conn);
    peripheral_conn = NULL;
    img_char_handle = 0;
    layer_char_handle = 0;
}

BT_CONN_CB_DEFINE(image_sync_conn_cb) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

static int image_sync_central_init(void) {
    BUILD_ASSERT(ART_IMAGES_COUNT >= 2,
                 "image_sync needs at least 2 images so left and right can differ");
    k_timer_init(&hour_timer, hour_tick, NULL);
    k_work_init_delayable(&discover_work, discover_work_cb);
    k_work_init(&push_img_work, push_img_work_cb);
    k_work_init(&push_layer_work, push_layer_work_cb);
    update_local();
    update_local_label();
    k_timer_start(&hour_timer, K_HOURS(1), K_HOURS(1));
    return 0;
}

SYS_INIT(image_sync_central_init, APPLICATION, 91);
