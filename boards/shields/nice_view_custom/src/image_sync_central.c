/*
 * Central side: owns the hourly rotation counter, updates its own displayed
 * index, and pushes the peripheral's index over a custom GATT characteristic
 * on the split connection.
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

#include "image_sync.h"
#include "../widgets/art/art_list.h"

LOG_MODULE_REGISTER(image_sync_central, CONFIG_ZMK_LOG_LEVEL);

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(IMAGE_SYNC_SERVICE_UUID);
static struct bt_uuid_128 chr_uuid = BT_UUID_INIT_128(IMAGE_SYNC_CHAR_UUID);

static struct bt_conn *peripheral_conn;
static uint16_t char_handle;
static struct bt_gatt_discover_params discover_params;

static uint32_t hour_counter;
static uint8_t local_idx;
static image_sync_listener_t listener_cb;

static struct k_timer hour_timer;
static struct k_work_delayable discover_work;
static struct k_work push_work;

static void push_peripheral_idx(void) {
    if (!peripheral_conn || !char_handle) {
        return;
    }
    uint8_t idx = (hour_counter + 1) % ART_IMAGES_COUNT;
    int err = bt_gatt_write_without_response(peripheral_conn, char_handle, &idx, 1, false);
    if (err) {
        LOG_WRN("image_sync write failed: %d", err);
    }
}

static void push_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    push_peripheral_idx();
}

static void update_local(void) {
    local_idx = hour_counter % ART_IMAGES_COUNT;
    if (listener_cb) {
        listener_cb(local_idx);
    }
}

uint8_t image_sync_get_index(void) { return local_idx; }

void image_sync_register_listener(image_sync_listener_t cb) {
    listener_cb = cb;
    if (cb) {
        cb(local_idx);
    }
}

static void hour_tick(struct k_timer *t) {
    ARG_UNUSED(t);
    hour_counter++;
    update_local();
    k_work_submit(&push_work);
}

static uint8_t char_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("image_sync char discovery done (handle=%u)", char_handle);
        return BT_GATT_ITER_STOP;
    }
    char_handle = bt_gatt_attr_value_handle(attr);
    LOG_DBG("image_sync char found, handle=%u", char_handle);
    k_work_submit(&push_work);
    return BT_GATT_ITER_STOP;
}

static uint8_t svc_discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               struct bt_gatt_discover_params *params) {
    if (!attr) {
        LOG_DBG("image_sync service not found on peripheral");
        return BT_GATT_ITER_STOP;
    }
    discover_params.uuid = &chr_uuid.uuid;
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = 0xffff;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = char_discover_cb;
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
    char_handle = 0;
    /* Let ZMK's split service finish its own discovery first. */
    k_work_schedule(&discover_work, K_SECONDS(3));
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason) {
    if (conn != peripheral_conn) {
        return;
    }
    bt_conn_unref(peripheral_conn);
    peripheral_conn = NULL;
    char_handle = 0;
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
    k_work_init(&push_work, push_work_cb);
    update_local();
    k_timer_start(&hour_timer, K_HOURS(1), K_HOURS(1));
    return 0;
}

SYS_INIT(image_sync_central_init, APPLICATION, 91);
