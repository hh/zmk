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

/*
 * Target slots. Each slot remembers ONE dongle by BLE address, so selecting a
 * slot always reaches the same machine instead of whichever dongle answers
 * first. A slot with no address is "open": the next dongle found while it is
 * selected gets bound to it -- the same model as ZMK's BLE profiles, minus the
 * bond (an InputStick needs no pairing, so it costs no BT_MAX_PAIRED slot).
 */

/* Select a slot and begin connecting to it. Binds on first sighting if open. */
int inputstick_select_slot(uint8_t slot);

/* Forget the dongle bound to a slot; it becomes open again. */
int inputstick_clear_slot(uint8_t slot);

/* The slot currently selected for output. */
uint8_t inputstick_active_slot(void);

/* True if the slot has a dongle address stored. Drives the status LED: an
 * open slot fast-blinks the way an unpaired BLE profile does. */
bool inputstick_slot_is_bound(uint8_t slot);

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
