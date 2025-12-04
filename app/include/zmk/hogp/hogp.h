/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/**
 * @brief Enter HOGP pairing mode
 *
 * Starts BLE scanning for HID devices. Will timeout after
 * CONFIG_ZMK_HOGP_SCAN_DURATION_SEC seconds.
 */
void hogp_enter_pairing_mode(void);

/**
 * @brief Exit HOGP pairing mode
 *
 * Stops scanning for HID devices.
 */
void hogp_exit_pairing_mode(void);

/**
 * @brief Print HOGP status to log
 *
 * Logs current pairing mode, scanning state, and connected devices.
 */
void hogp_print_status(void);

/**
 * @brief Clear HOGP device bonds only
 *
 * Disconnects any connected HOGP devices and removes their bonds.
 * Host device bonds (laptops, etc.) are preserved.
 */
void hogp_clear_bonds(void);

/**
 * @brief Clear host device bonds only
 *
 * Removes bonds for host devices (laptops, etc.).
 * HOGP device bonds (mice, trackpads) are preserved.
 */
void hogp_clear_host_bonds(void);

/**
 * @brief Callback type for received HID reports
 *
 * @param data Pointer to HID report data
 * @param len Length of HID report in bytes
 */
typedef void (*hogp_report_callback_t)(const uint8_t *data, uint16_t len);

/**
 * @brief Register callback for HID reports
 *
 * The callback will be invoked for each HID report received from
 * connected HOGP devices. Only one callback can be registered at a time.
 *
 * @param cb Callback function, or NULL to unregister
 */
void hogp_register_report_callback(hogp_report_callback_t cb);

/**
 * @brief Dump NVS settings state for debugging
 *
 * Logs the current state of NVS-stored HOGP device addresses.
 */
void hogp_dump_nvs_state(void);
