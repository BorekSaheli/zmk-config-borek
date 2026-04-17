/*
 * Peripheral side of the image-sync service: exposes a single-byte write
 * characteristic. Central writes the desired image index; we store it and
 * notify the registered listener (the widget).
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "image_sync.h"
#include "../widgets/art/art_list.h"

LOG_MODULE_REGISTER(image_sync_peripheral, CONFIG_ZMK_LOG_LEVEL);

/* Default so a freshly-booted peripheral doesn't match a freshly-booted
 * central (which starts at index 0) while waiting for the first write. */
static uint8_t current_idx = 1;
static image_sync_listener_t listener_cb;

static ssize_t write_idx_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset != 0 || len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    uint8_t idx = *(const uint8_t *)buf;
    if (idx >= ART_IMAGES_COUNT) {
        /* Ignore out-of-range values rather than erroring; central may
         * have a newer image list than we do during partial flashes. */
        idx = 0;
    }
    if (idx != current_idx) {
        current_idx = idx;
        if (listener_cb) {
            listener_cb(current_idx);
        }
    }
    return len;
}

BT_GATT_SERVICE_DEFINE(image_sync_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(IMAGE_SYNC_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(IMAGE_SYNC_CHAR_UUID),
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE_ENCRYPT,
                           NULL, write_idx_cb, NULL),
);

uint8_t image_sync_get_index(void) { return current_idx; }

void image_sync_register_listener(image_sync_listener_t cb) {
    listener_cb = cb;
    if (cb) {
        cb(current_idx);
    }
}

static int image_sync_peripheral_init(void) {
    BUILD_ASSERT(ART_IMAGES_COUNT >= 2,
                 "image_sync needs at least 2 images so left and right can differ");
    return 0;
}

SYS_INIT(image_sync_peripheral_init, APPLICATION, 91);
