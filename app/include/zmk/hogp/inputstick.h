/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * InputStick client: drive an InputStick USB HID dongle over BLE.
 *
 * The keyboard acts as a BLE central and talks to the dongle over Nordic
 * UART Service (NOT HOGP -- the dongle is not a HID peripheral). Whatever
 * machine the dongle is plugged into sees a plain USB keyboard/mouse, so
 * the Adv360 can type into a host it has never paired with.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Begin scanning for a dongle advertising as "InputStick*" and connect. */
void inputstick_start(void);

/* Disconnect and stop scanning. */
void inputstick_stop(void);

/* Log connection state, firmware version, and handles. */
void inputstick_print_status(void);

/* True once the init handshake has completed and HID data will be accepted. */
bool inputstick_is_ready(void);

/*
 * Queue an ASCII string to be typed on the dongle (press/release per char).
 * Returns 0 if queued, -ENOTCONN if not ready, -EAGAIN if the queue is full.
 */
int inputstick_type(const char *text);

/*
 * Send a full held-key state: modifier bitmap plus up to 6 concurrent
 * non-modifier HID keycodes (extras are dropped, no rollover-overflow
 * report). Use this for live key mirroring rather than inputstick_type().
 */
int inputstick_send_keys(uint8_t modifier, const uint8_t *keycodes, size_t count);

/* Relative mouse report. x/y/wheel are signed -128..127. */
int inputstick_send_mouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);
