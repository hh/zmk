/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* For IS_ENABLED() -- this header is included from places that do not already
 * pull in the Zephyr macro utilities. */
#include <zephyr/sys/util_macro.h>

/**
 * The method by which data is sent.
 */
enum zmk_transport {
    ZMK_TRANSPORT_USB,
    ZMK_TRANSPORT_BLE,
#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    /*
     * Output through an InputStick USB HID dongle, which the keyboard drives
     * as a BLE central. The target machine sees a plain USB keyboard/mouse and
     * never pairs with us at all.
     *
     * Deliberately NOT modelled as extra BLE profiles: ZMK profiles are slots
     * in the bond table (ZMK_BLE_PROFILE_COUNT is derived from
     * CONFIG_BT_MAX_PAIRED), and an InputStick needs no bond, so a dongle in
     * that array would burn a scarce bond slot to store nothing.
     */
    ZMK_TRANSPORT_INPUTSTICK,
#endif
};

/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_USB.
 */
struct zmk_transport_usb_data {};

/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_BLE.
 */
struct zmk_transport_ble_data {
    int profile_index;
};

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
/**
 * Configuration to select an endpoint on ZMK_TRANSPORT_INPUTSTICK.
 *
 * A slot remembers one specific dongle by BLE address, so switching to slot N
 * always reaches the same machine rather than whichever dongle answers first.
 */
struct zmk_transport_inputstick_data {
    int slot;
};
#endif

/**
 * A specific endpoint to which data may be sent.
 */
struct zmk_endpoint_instance {
    enum zmk_transport transport;
    union {
        struct zmk_transport_usb_data usb; // ZMK_TRANSPORT_USB
        struct zmk_transport_ble_data ble; // ZMK_TRANSPORT_BLE
#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
        struct zmk_transport_inputstick_data inputstick; // ZMK_TRANSPORT_INPUTSTICK
#endif
    };
};
