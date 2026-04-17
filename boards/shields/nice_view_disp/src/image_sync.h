/*
 * Rotating-image sync between the two halves of a split keyboard.
 * Central owns an hour_counter; it sets its own index to
 *   idx_left  =  hour_counter              % N
 * and pushes to peripheral
 *   idx_right = (hour_counter + 1)         % N
 * so the two halves are always different (N >= 2 required).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* UUIDs — random 128-bit values, not overlapping with ZMK's split service. */
#define IMAGE_SYNC_SERVICE_UUID                                                                    \
    BT_UUID_128_ENCODE(0xf1a4b000, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)
#define IMAGE_SYNC_CHAR_UUID                                                                       \
    BT_UUID_128_ENCODE(0xf1a4b001, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)

typedef void (*image_sync_listener_t)(uint8_t idx);

/* Register a callback invoked when the local side's image index changes.
 * Only one listener is supported (the widget). Safe to call during init. */
void image_sync_register_listener(image_sync_listener_t cb);

/* Last-known index for this side. Returns 0 before the first sync. */
uint8_t image_sync_get_index(void);
