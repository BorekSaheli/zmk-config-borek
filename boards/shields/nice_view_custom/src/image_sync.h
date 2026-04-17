/*
 * Split-sync of the current image index between central and peripheral.
 * Central owns an hour_counter: on each tick it picks
 *   idx_left  =  hour_counter        % N   (its own displayed index)
 *   idx_right = (hour_counter + 1)   % N   (pushed to peripheral)
 * so the two halves are always different (requires N >= 2).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/* 128-bit UUIDs — random values, chosen to not clash with ZMK's split service. */
#define IMAGE_SYNC_SERVICE_UUID                                                                    \
    BT_UUID_128_ENCODE(0xf1a4b000, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)
#define IMAGE_SYNC_CHAR_UUID                                                                       \
    BT_UUID_128_ENCODE(0xf1a4b001, 0x6b1f, 0x4d6e, 0xa5b4, 0x5a6e2f4b6a01)

typedef void (*image_sync_listener_t)(uint8_t idx);

/* Register a callback fired whenever the local side's image index changes.
 * Only one listener is supported. The callback is also invoked once during
 * registration with the current cached index. */
void image_sync_register_listener(image_sync_listener_t cb);

/* Current index for this side. 0 before any update has happened. */
uint8_t image_sync_get_index(void);
