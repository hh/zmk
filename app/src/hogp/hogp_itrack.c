/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * HOGP iTrack Handler - Brydge iTrack ADG Trackpad
 *
 * The Brydge iTrack uses Apple ADG (Accessory Design Guidelines) format.
 * This module does pure passthrough with only the confidence fix applied.
 *
 * The iTrack has a firmware quirk: it reports confidence=0 when fingers
 * are close together (e.g., starting a pinch gesture). We fix this by
 * forcing confidence=1 whenever tip=1.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/hogp/hogp.h>
#include <zmk/hid.h>
#include <zmk/hog.h>
#include <zmk/endpoints.h>

LOG_MODULE_REGISTER(hogp_itrack, CONFIG_ZMK_HOGP_LOG_LEVEL);

/*
 * iTrack report formats (after BLE strips report ID):
 *   20 bytes: Just fingers (5 × 4 bytes) - no scan time or buttons
 *   23 bytes: Scan time (2) + buttons (1) + fingers (20)
 */
#define ITRACK_REPORT_LEN_FINGERS   20
#define ITRACK_REPORT_LEN_FULL      23
#define ITRACK_MAX_FINGERS          5

/* Finger byte 0 bit masks */
#define FINGER_TIP_MASK         0x01
#define FINGER_CONFIDENCE_MASK  0x02

static uint16_t itrack_scan_time = 0;

/*
 * Process iTrack ADG report - pure passthrough with confidence fix
 */
static void hogp_itrack_process_report(const uint8_t *data, uint16_t len)
{
    struct zmk_hid_trackpad_report *report = zmk_hid_get_trackpad_report();
    const uint8_t *finger_data;

    if (len == ITRACK_REPORT_LEN_FINGERS) {
        /* BLE sends just 5 fingers (20 bytes), no scan time or buttons */
        report->body.scan_time = itrack_scan_time;
        report->body.buttons = 0;
        finger_data = data;
        itrack_scan_time += 80;
    } else if (len == ITRACK_REPORT_LEN_FULL) {
        /* Full format: scan_time(2) + buttons(1) + fingers(20) */
        report->body.scan_time = *(uint16_t *)&data[0];
        report->body.buttons = data[2];
        finger_data = &data[3];
    } else {
        LOG_WRN("iTrack unexpected length: %d", len);
        return;
    }

    /* Pure passthrough - copy finger data directly */
    memcpy(report->body.fingers, finger_data,
           ITRACK_MAX_FINGERS * sizeof(struct zmk_hid_trackpad_finger));

    /* Apply confidence fix: force confidence=1 when tip=1 */
    for (int i = 0; i < ITRACK_MAX_FINGERS; i++) {
        struct zmk_hid_trackpad_finger *f = &report->body.fingers[i];
        if (f->tip_conf_idx & FINGER_TIP_MASK) {
            f->tip_conf_idx |= FINGER_CONFIDENCE_MASK;
        }
    }

    /* Send to host */
    int ret = zmk_endpoints_send_trackpad_report();
    if (ret < 0) {
        LOG_WRN("Failed to send trackpad report: %d", ret);
    }
}

static int hogp_itrack_init(void)
{
    LOG_INF("HOGP iTrack handler ready (passthrough + confidence fix)");
    zmk_hid_trackpad_clear();
    hogp_register_report_callback(hogp_itrack_process_report);
    return 0;
}

SYS_INIT(hogp_itrack_init, APPLICATION, 92);
