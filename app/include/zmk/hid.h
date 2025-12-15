/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/sys/util.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/keys.h>
#if IS_ENABLED(CONFIG_ZMK_POINTING)
#include <zmk/pointing.h>
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

#if IS_ENABLED(CONFIG_ZMK_HID_KEYBOARD_NKRO_EXTENDED_REPORT)
#define ZMK_HID_KEYBOARD_NKRO_MAX_USAGE HID_USAGE_KEY_KEYBOARD_LANG8
#else
#define ZMK_HID_KEYBOARD_NKRO_MAX_USAGE HID_USAGE_KEY_KEYPAD_EQUAL
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_CONSUMER_REPORT_USAGES_BASIC)
#define ZMK_HID_CONSUMER_MAX_USAGE 0xFF
#elif IS_ENABLED(CONFIG_ZMK_HID_CONSUMER_REPORT_USAGES_FULL)
#define ZMK_HID_CONSUMER_MAX_USAGE 0xFFF
#else
#error "Unknown consumer report usages configuration"
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_NKRO)
#define ZMK_HID_KEYBOARD_MAX_USAGE ZMK_HID_KEYBOARD_NKRO_MAX_USAGE
#elif IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_HKRO)
#define ZMK_HID_KEYBOARD_MAX_USAGE 0xFF
#else
#error "Unknown keyboard report usages configuration"
#endif

#define ZMK_HID_MOUSE_NUM_BUTTONS 0x05

// See https://www.usb.org/sites/default/files/hid1_11.pdf section 6.2.2.4 Main Items

#define ZMK_HID_MAIN_VAL_DATA (0x00 << 0)
#define ZMK_HID_MAIN_VAL_CONST (0x01 << 0)

#define ZMK_HID_MAIN_VAL_ARRAY (0x00 << 1)
#define ZMK_HID_MAIN_VAL_VAR (0x01 << 1)

#define ZMK_HID_MAIN_VAL_ABS (0x00 << 2)
#define ZMK_HID_MAIN_VAL_REL (0x01 << 2)

#define ZMK_HID_MAIN_VAL_NO_WRAP (0x00 << 3)
#define ZMK_HID_MAIN_VAL_WRAP (0x01 << 3)

#define ZMK_HID_MAIN_VAL_LIN (0x00 << 4)
#define ZMK_HID_MAIN_VAL_NON_LIN (0x01 << 4)

#define ZMK_HID_MAIN_VAL_PREFERRED (0x00 << 5)
#define ZMK_HID_MAIN_VAL_NO_PREFERRED (0x01 << 5)

#define ZMK_HID_MAIN_VAL_NO_NULL (0x00 << 6)
#define ZMK_HID_MAIN_VAL_NULL (0x01 << 6)

#define ZMK_HID_MAIN_VAL_NON_VOL (0x00 << 7)
#define ZMK_HID_MAIN_VAL_VOL (0x01 << 7)

#define ZMK_HID_MAIN_VAL_BIT_FIELD (0x00 << 8)
#define ZMK_HID_MAIN_VAL_BUFFERED_BYTES (0x01 << 8)

#define ZMK_HID_REPORT_ID_KEYBOARD 0x01
#define ZMK_HID_REPORT_ID_LEDS 0x01
#define ZMK_HID_REPORT_ID_CONSUMER 0x02
#define ZMK_HID_REPORT_ID_MOUSE 0x03
#define ZMK_HID_REPORT_ID_TRACKPAD 0x04

#ifndef HID_ITEM_TAG_PUSH
#define HID_ITEM_TAG_PUSH 0xA
#endif

#ifndef HID_ITEM_TAG_POP
#define HID_ITEM_TAG_POP 0xB
#endif

#define HID_PUSH HID_ITEM(HID_ITEM_TAG_PUSH, HID_ITEM_TYPE_GLOBAL, 0)

#define HID_POP HID_ITEM(HID_ITEM_TAG_POP, HID_ITEM_TYPE_GLOBAL, 0)

#ifndef HID_PHYSICAL_MIN8
#define HID_PHYSICAL_MIN8(a) HID_ITEM(HID_ITEM_TAG_PHYSICAL_MIN, HID_ITEM_TYPE_GLOBAL, 1), a
#endif

#ifndef HID_PHYSICAL_MAX8
#define HID_PHYSICAL_MAX8(a) HID_ITEM(HID_ITEM_TAG_PHYSICAL_MAX, HID_ITEM_TYPE_GLOBAL, 1), a
#endif

#define HID_USAGE16(a, b) HID_ITEM(HID_ITEM_TAG_USAGE, HID_ITEM_TYPE_LOCAL, 2), a, b

#define HID_USAGE16_SINGLE(a) HID_USAGE16((a & 0xFF), ((a >> 8) & 0xFF))

static const uint8_t zmk_hid_report_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_HID_REPORT_ID_KEYBOARD),
    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_USAGE_MIN8(HID_USAGE_KEY_KEYBOARD_LEFTCONTROL),
    HID_USAGE_MAX8(HID_USAGE_KEY_KEYBOARD_RIGHT_GUI),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x01),

    HID_REPORT_SIZE(0x01),
    HID_REPORT_COUNT(0x08),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),

    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)

    HID_USAGE_PAGE(HID_USAGE_LED),
    HID_USAGE_MIN8(HID_USAGE_LED_NUM_LOCK),
    HID_USAGE_MAX8(HID_USAGE_LED_KANA),
    HID_REPORT_SIZE(0x01),
    HID_REPORT_COUNT(0x05),
    HID_OUTPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),

    HID_USAGE_PAGE(HID_USAGE_LED),
    HID_REPORT_SIZE(0x03),
    HID_REPORT_COUNT(0x01),
    HID_OUTPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),

#endif // IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)

    HID_USAGE_PAGE(HID_USAGE_KEY),

#if IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_NKRO)
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x01),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX8(ZMK_HID_KEYBOARD_NKRO_MAX_USAGE),
    HID_REPORT_SIZE(0x01),
    HID_REPORT_COUNT(ZMK_HID_KEYBOARD_NKRO_MAX_USAGE + 1),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
#elif IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_HKRO)
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX8(0xFF),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(CONFIG_ZMK_HID_KEYBOARD_REPORT_SIZE),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_ARRAY | ZMK_HID_MAIN_VAL_ABS),
#else
#error "A proper HID report type must be selected"
#endif

    HID_END_COLLECTION,
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),
    HID_USAGE(HID_USAGE_CONSUMER_CONSUMER_CONTROL),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_HID_REPORT_ID_CONSUMER),
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),

#if IS_ENABLED(CONFIG_ZMK_HID_CONSUMER_REPORT_USAGES_BASIC)
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX8(0xFF),
    HID_REPORT_SIZE(0x08),
#elif IS_ENABLED(CONFIG_ZMK_HID_CONSUMER_REPORT_USAGES_FULL)
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x0F),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX16(0xFF, 0x0F),
    HID_REPORT_SIZE(0x10),
#else
#error "A proper consumer HID report usage range must be selected"
#endif
    HID_REPORT_COUNT(CONFIG_ZMK_HID_CONSUMER_REPORT_SIZE),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_ARRAY | ZMK_HID_MAIN_VAL_ABS),
    HID_END_COLLECTION,

#if IS_ENABLED(CONFIG_ZMK_POINTING)
    HID_USAGE_PAGE(HID_USAGE_GD),
    HID_USAGE(HID_USAGE_GD_MOUSE),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(ZMK_HID_REPORT_ID_MOUSE),
    HID_USAGE(HID_USAGE_GD_POINTER),
    HID_COLLECTION(HID_COLLECTION_PHYSICAL),
    HID_USAGE_PAGE(HID_USAGE_BUTTON),
    HID_USAGE_MIN8(0x1),
    HID_USAGE_MAX8(ZMK_HID_MOUSE_NUM_BUTTONS),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x01),
    HID_REPORT_SIZE(0x01),
    HID_REPORT_COUNT(0x5),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Constant padding for the last 3 bits.
    HID_REPORT_SIZE(0x03),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    // Some OSes ignore pointer devices without X/Y data.
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_X),
    HID_USAGE(HID_USAGE_GD_Y),
    HID_LOGICAL_MIN16(0xFF, -0x7F),
    HID_LOGICAL_MAX16(0xFF, 0x7F),
    HID_REPORT_SIZE(0x10),
    HID_REPORT_COUNT(0x02),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_REL),
    HID_COLLECTION(HID_COLLECTION_LOGICAL),
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
    HID_USAGE(HID_USAGE_GD_RESOLUTION_MULTIPLIER),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX8(0x0F),
    HID_PHYSICAL_MIN8(0x01),
    HID_PHYSICAL_MAX8(0x10),
    HID_REPORT_SIZE(0x04),
    HID_REPORT_COUNT(0x01),
    HID_PUSH,
    HID_FEATURE(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
#endif // IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
    HID_USAGE(HID_USAGE_GD_WHEEL),
    HID_LOGICAL_MIN16(0xFF, -0x7F),
    HID_LOGICAL_MAX16(0xFF, 0x7F),
    HID_PHYSICAL_MIN8(0x00),
    HID_PHYSICAL_MAX8(0x00),
    HID_REPORT_SIZE(0x10),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_REL),
    HID_END_COLLECTION,
    HID_COLLECTION(HID_COLLECTION_LOGICAL),
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
    HID_USAGE(HID_USAGE_GD_RESOLUTION_MULTIPLIER),
    HID_POP,
    HID_FEATURE(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
#endif // IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),
    HID_USAGE16_SINGLE(HID_USAGE_CONSUMER_AC_PAN),
    HID_LOGICAL_MIN16(0xFF, -0x7F),
    HID_LOGICAL_MAX16(0xFF, 0x7F),
    HID_PHYSICAL_MIN8(0x00),
    HID_PHYSICAL_MAX8(0x00),
    HID_REPORT_SIZE(0x10),
    HID_REPORT_COUNT(0x01),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_REL),
    HID_END_COLLECTION,
    HID_END_COLLECTION,
    HID_END_COLLECTION,
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

#if IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT)
    /* ADG Chapter 15 Trackpad - Touch Pad Application
     * For USB with CONFIG_ZMK_USB_HID_TRACKPAD_INTERFACE: trackpad is on HID_1
     * For BLE: trackpad is included in this descriptor
     */
    0x05, 0x0D,             /* USAGE_PAGE (Digitizers) */
    0x09, 0x05,             /* USAGE (Touch Pad) */
    0xA1, 0x01,             /* COLLECTION (Application) */
    0x85, ZMK_HID_REPORT_ID_TRACKPAD, /* REPORT_ID (4) */

    /* Scan Time (16-bit, 100us units) */
    0x05, 0x0D,             /*   USAGE_PAGE (Digitizers) */
    0x09, 0x56,             /*   USAGE (Scan Time) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x27, 0xFF, 0xFF, 0x00, 0x00, /* LOGICAL_MAXIMUM (65535) */
    0x75, 0x10,             /*   REPORT_SIZE (16) */
    0x95, 0x01,             /*   REPORT_COUNT (1) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */

    /* Buttons (2 bits + 6 padding) */
    0x05, 0x09,             /*   USAGE_PAGE (Button) */
    0x19, 0x01,             /*   USAGE_MINIMUM (Button 1) */
    0x29, 0x02,             /*   USAGE_MAXIMUM (Button 2) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,             /*   LOGICAL_MAXIMUM (1) */
    0x75, 0x01,             /*   REPORT_SIZE (1) */
    0x95, 0x02,             /*   REPORT_COUNT (2) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x95, 0x06,             /*   REPORT_COUNT (6) */
    0x81, 0x01,             /*   INPUT (Const) - padding */

    /* Finger 0 */
    0x05, 0x0D,             /* USAGE_PAGE (Digitizers) */
    0x09, 0x22,             /* USAGE (Finger) */
    0xA1, 0x02,             /* COLLECTION (Logical) */
    0x09, 0x42,             /*   USAGE (Tip Switch) */
    0x09, 0x47,             /*   USAGE (Confidence) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,             /*   LOGICAL_MAXIMUM (1) */
    0x75, 0x01,             /*   REPORT_SIZE (1) */
    0x95, 0x02,             /*   REPORT_COUNT (2) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x09, 0x38,             /*   USAGE (Transducer Index) */
    0x25, 0x04,             /*   LOGICAL_MAXIMUM (4) */
    0x75, 0x06,             /*   REPORT_SIZE (6) */
    0x95, 0x01,             /*   REPORT_COUNT (1) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x05, 0x01,             /*   USAGE_PAGE (Generic Desktop) */
    0x09, 0x30,             /*   USAGE (X) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x26, 0x40, 0x06,       /*   LOGICAL_MAXIMUM (1600) - iTrack X range */
    0x35, 0x00,             /*   PHYSICAL_MINIMUM (0) */
    0x46, 0x50, 0x05,       /*   PHYSICAL_MAXIMUM (1360) = 136.0mm iTrack */
    0x55, 0x0E,             /*   UNIT_EXPONENT (-2) */
    0x65, 0x11,             /*   UNIT (cm) */
    0x75, 0x0C,             /*   REPORT_SIZE (12) */
    0x95, 0x01,             /*   REPORT_COUNT (1) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x09, 0x31,             /*   USAGE (Y) */
    0x26, 0xB6, 0x03,       /*   LOGICAL_MAXIMUM (950) - iTrack Y range */
    0x46, 0x5C, 0x03,       /*   PHYSICAL_MAXIMUM (860) = 86.0mm iTrack */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0xC0,                   /* END_COLLECTION */

    /* Finger 1 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    /* Finger 2 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    /* Finger 3 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    /* Finger 4 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    0xC0,                   /* END_COLLECTION (Application) */
#endif // CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT || CONFIG_ZMK_HOGP_ITRACK_OUTPUT
};

/* Size of the trackpad portion in zmk_hid_report_desc (for USB HID_0 to exclude it) */
#if IS_ENABLED(CONFIG_ZMK_USB_HID_TRACKPAD_INTERFACE) && \
    (IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT))
#define ZMK_HID_TRACKPAD_DESC_SIZE sizeof(zmk_hid_trackpad_desc)
#define ZMK_HID_REPORT_DESC_USB_SIZE (sizeof(zmk_hid_report_desc) - ZMK_HID_TRACKPAD_DESC_SIZE)
#else
#define ZMK_HID_REPORT_DESC_USB_SIZE sizeof(zmk_hid_report_desc)
#endif

/*
 * Standalone trackpad descriptor for USB HID_1 interface.
 * Used when CONFIG_ZMK_USB_HID_TRACKPAD_INTERFACE is enabled.
 * This allows hid-multitouch to bind to trackpad while hid-generic handles keyboard/mouse.
 */
#if IS_ENABLED(CONFIG_ZMK_USB_HID_TRACKPAD_INTERFACE) && \
    (IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT))
static const uint8_t zmk_hid_trackpad_desc[] = {
    /* ADG Chapter 15 Trackpad - Touch Pad Application */
    0x05, 0x0D,             /* USAGE_PAGE (Digitizers) */
    0x09, 0x05,             /* USAGE (Touch Pad) */
    0xA1, 0x01,             /* COLLECTION (Application) */
    0x85, ZMK_HID_REPORT_ID_TRACKPAD, /* REPORT_ID (4) */

    /* Scan Time (16-bit, 100us units) */
    0x05, 0x0D,             /*   USAGE_PAGE (Digitizers) */
    0x09, 0x56,             /*   USAGE (Scan Time) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x27, 0xFF, 0xFF, 0x00, 0x00, /* LOGICAL_MAXIMUM (65535) */
    0x75, 0x10,             /*   REPORT_SIZE (16) */
    0x95, 0x01,             /*   REPORT_COUNT (1) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */

    /* Buttons (2 bits + 6 padding) */
    0x05, 0x09,             /*   USAGE_PAGE (Button) */
    0x19, 0x01,             /*   USAGE_MINIMUM (Button 1) */
    0x29, 0x02,             /*   USAGE_MAXIMUM (Button 2) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,             /*   LOGICAL_MAXIMUM (1) */
    0x75, 0x01,             /*   REPORT_SIZE (1) */
    0x95, 0x02,             /*   REPORT_COUNT (2) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x95, 0x06,             /*   REPORT_COUNT (6) */
    0x81, 0x01,             /*   INPUT (Const) - padding */

    /* Finger 0 */
    0x05, 0x0D,             /* USAGE_PAGE (Digitizers) */
    0x09, 0x22,             /* USAGE (Finger) */
    0xA1, 0x02,             /* COLLECTION (Logical) */
    0x09, 0x42,             /*   USAGE (Tip Switch) */
    0x09, 0x47,             /*   USAGE (Confidence) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,             /*   LOGICAL_MAXIMUM (1) */
    0x75, 0x01,             /*   REPORT_SIZE (1) */
    0x95, 0x02,             /*   REPORT_COUNT (2) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x09, 0x38,             /*   USAGE (Transducer Index) */
    0x25, 0x04,             /*   LOGICAL_MAXIMUM (4) */
    0x75, 0x06,             /*   REPORT_SIZE (6) */
    0x95, 0x01,             /*   REPORT_COUNT (1) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x05, 0x01,             /*   USAGE_PAGE (Generic Desktop) */
    0x09, 0x30,             /*   USAGE (X) */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0) */
    0x26, 0x40, 0x06,       /*   LOGICAL_MAXIMUM (1600) - iTrack X range */
    0x35, 0x00,             /*   PHYSICAL_MINIMUM (0) */
    0x46, 0x50, 0x05,       /*   PHYSICAL_MAXIMUM (1360) = 136.0mm iTrack */
    0x55, 0x0E,             /*   UNIT_EXPONENT (-2) */
    0x65, 0x11,             /*   UNIT (cm) */
    0x75, 0x0C,             /*   REPORT_SIZE (12) */
    0x95, 0x01,             /*   REPORT_COUNT (1) */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0x09, 0x31,             /*   USAGE (Y) */
    0x26, 0xB6, 0x03,       /*   LOGICAL_MAXIMUM (950) - iTrack Y range */
    0x46, 0x5C, 0x03,       /*   PHYSICAL_MAXIMUM (860) = 86.0mm iTrack */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs) */
    0xC0,                   /* END_COLLECTION */

    /* Finger 1 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    /* Finger 2 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    /* Finger 3 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    /* Finger 4 */
    0x05, 0x0D,
    0x09, 0x22,
    0xA1, 0x02,
    0x09, 0x42,
    0x09, 0x47,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x02,
    0x81, 0x02,
    0x09, 0x38,
    0x25, 0x04,
    0x75, 0x06,
    0x95, 0x01,
    0x81, 0x02,
    0x05, 0x01,
    0x09, 0x30,
    0x15, 0x00,
    0x26, 0x40, 0x06,       /* X LOGICAL_MAX (1600) */
    0x35, 0x00,
    0x46, 0x50, 0x05,       /* X PHYSICAL_MAX (1360) = 136mm */
    0x55, 0x0E,
    0x65, 0x11,
    0x75, 0x0C,
    0x95, 0x01,
    0x81, 0x02,
    0x09, 0x31,
    0x26, 0xB6, 0x03,       /* Y LOGICAL_MAX (950) */
    0x46, 0x5C, 0x03,       /* Y PHYSICAL_MAX (860) = 86mm */
    0x81, 0x02,
    0xC0,

    0xC0,                   /* END_COLLECTION (Application) */
};
#endif // CONFIG_ZMK_USB_HID_TRACKPAD_INTERFACE

#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)

#define HID_ERROR_ROLLOVER 0x1
#define HID_BOOT_KEY_LEN 6

#if IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_HKRO) &&                                                 \
    CONFIG_ZMK_HID_KEYBOARD_REPORT_SIZE == HID_BOOT_KEY_LEN
typedef struct zmk_hid_keyboard_report_body zmk_hid_boot_report_t;
#else
struct zmk_hid_boot_report {
    zmk_mod_flags_t modifiers;
    uint8_t _reserved;
    uint8_t keys[HID_BOOT_KEY_LEN];
} __packed;

typedef struct zmk_hid_boot_report zmk_hid_boot_report_t;
#endif
#endif

struct zmk_hid_keyboard_report_body {
    zmk_mod_flags_t modifiers;
    uint8_t _reserved;
#if IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_NKRO)
    uint8_t keys[DIV_ROUND_UP(ZMK_HID_KEYBOARD_NKRO_MAX_USAGE + 1, 8)];
#elif IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_HKRO)
    uint8_t keys[CONFIG_ZMK_HID_KEYBOARD_REPORT_SIZE];
#endif
} __packed;

struct zmk_hid_keyboard_report {
    uint8_t report_id;
    struct zmk_hid_keyboard_report_body body;
} __packed;

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)

struct zmk_hid_led_report_body {
    uint8_t leds;
} __packed;

struct zmk_hid_led_report {
    uint8_t report_id;
    struct zmk_hid_led_report_body body;
} __packed;

#endif // IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)

struct zmk_hid_consumer_report_body {
#if IS_ENABLED(CONFIG_ZMK_HID_CONSUMER_REPORT_USAGES_BASIC)
    uint8_t keys[CONFIG_ZMK_HID_CONSUMER_REPORT_SIZE];
#elif IS_ENABLED(CONFIG_ZMK_HID_CONSUMER_REPORT_USAGES_FULL)
    uint16_t keys[CONFIG_ZMK_HID_CONSUMER_REPORT_SIZE];
#endif
} __packed;

struct zmk_hid_consumer_report {
    uint8_t report_id;
    struct zmk_hid_consumer_report_body body;
} __packed;

#if IS_ENABLED(CONFIG_ZMK_POINTING)
struct zmk_hid_mouse_report_body {
    zmk_mouse_button_flags_t buttons;
    int16_t d_x;
    int16_t d_y;
    int16_t d_scroll_y;
    int16_t d_scroll_x;
} __packed;

struct zmk_hid_mouse_report {
    uint8_t report_id;
    struct zmk_hid_mouse_report_body body;
} __packed;

#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)

struct zmk_hid_mouse_resolution_feature_report_body {
    uint8_t wheel_res : 4;
    uint8_t hwheel_res : 4;
} __packed;

struct zmk_hid_mouse_resolution_feature_report {
    uint8_t report_id;
    struct zmk_hid_mouse_resolution_feature_report_body body;
} __packed;

#endif // IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)

#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

#if IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT)

/* Maximum fingers supported in ADG trackpad report */
#define ZMK_HID_TRACKPAD_MAX_FINGERS 5

/*
 * ADG Finger Data (4 bytes packed)
 *
 * Bit layout (matches USB HID descriptor):
 *   Byte 0: Tip Switch (1) | Confidence (1) | Transducer Index (6)
 *   Byte 1: X[7:0]
 *   Byte 2: X[11:8] (low nibble) | Y[3:0] (high nibble)
 *   Byte 3: Y[11:4]
 */
struct zmk_hid_trackpad_finger {
    uint8_t tip_conf_idx;   /* Tip(1) + Confidence(1) + TransducerIndex(6) */
    uint8_t x_low;          /* X[7:0] */
    uint8_t x_high_y_low;   /* X[11:8] | Y[3:0] */
    uint8_t y_high;         /* Y[11:4] */
} __packed;

/*
 * ADG Trackpad Input Report (24 bytes)
 *
 * Report ID 4 for ZMK (avoids conflict with mouse ID 3)
 */
struct zmk_hid_trackpad_report_body {
    uint16_t scan_time;             /* Relative timestamp, 100us units */
    uint8_t buttons;                /* Button1(1) + Button2(1) + pad(6) */
    struct zmk_hid_trackpad_finger fingers[ZMK_HID_TRACKPAD_MAX_FINGERS];
} __packed;

struct zmk_hid_trackpad_report {
    uint8_t report_id;
    struct zmk_hid_trackpad_report_body body;
} __packed;

/* Verify struct sizes at compile time */
_Static_assert(sizeof(struct zmk_hid_trackpad_finger) == 4, "trackpad_finger must be 4 bytes");
_Static_assert(sizeof(struct zmk_hid_trackpad_report) == 24, "trackpad_report must be 24 bytes");

/*
 * Set finger data from coordinates
 *
 * @param f      Pointer to finger struct
 * @param id     Transducer index (0-4)
 * @param tip    Tip switch (finger touching)
 * @param conf   Confidence (valid contact)
 * @param x      X coordinate (0-2557)
 * @param y      Y coordinate (0-1154)
 */
static inline void zmk_hid_trackpad_finger_set(struct zmk_hid_trackpad_finger *f,
                                               uint8_t id, bool tip, bool conf,
                                               uint16_t x, uint16_t y)
{
    f->tip_conf_idx = (tip ? 0x01 : 0x00) |
                      (conf ? 0x02 : 0x00) |
                      ((id & 0x1F) << 2);
    f->x_low = x & 0xFF;
    f->x_high_y_low = ((x >> 8) & 0x0F) | ((y & 0x0F) << 4);
    f->y_high = (y >> 4) & 0xFF;
}

/*
 * Clear finger slot (no contact)
 */
static inline void zmk_hid_trackpad_finger_clear(struct zmk_hid_trackpad_finger *f)
{
    f->tip_conf_idx = 0;
    f->x_low = 0;
    f->x_high_y_low = 0;
    f->y_high = 0;
}

#endif // IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT)

zmk_mod_flags_t zmk_hid_get_explicit_mods(void);
int zmk_hid_register_mod(zmk_mod_t modifier);
int zmk_hid_unregister_mod(zmk_mod_t modifier);
bool zmk_hid_mod_is_pressed(zmk_mod_t modifier);

int zmk_hid_register_mods(zmk_mod_flags_t explicit_modifiers);
int zmk_hid_unregister_mods(zmk_mod_flags_t explicit_modifiers);
int zmk_hid_implicit_modifiers_press(zmk_mod_flags_t implicit_modifiers);
int zmk_hid_implicit_modifiers_release(void);
int zmk_hid_masked_modifiers_set(zmk_mod_flags_t masked_modifiers);
int zmk_hid_masked_modifiers_clear(void);

int zmk_hid_keyboard_press(zmk_key_t key);
int zmk_hid_keyboard_release(zmk_key_t key);
void zmk_hid_keyboard_clear(void);
bool zmk_hid_keyboard_is_pressed(zmk_key_t key);

int zmk_hid_consumer_press(zmk_key_t key);
int zmk_hid_consumer_release(zmk_key_t key);
void zmk_hid_consumer_clear(void);
bool zmk_hid_consumer_is_pressed(zmk_key_t key);

int zmk_hid_press(uint32_t usage);
int zmk_hid_release(uint32_t usage);
bool zmk_hid_is_pressed(uint32_t usage);

#if IS_ENABLED(CONFIG_ZMK_POINTING)
int zmk_hid_mouse_button_press(zmk_mouse_button_t button);
int zmk_hid_mouse_button_release(zmk_mouse_button_t button);
int zmk_hid_mouse_buttons_press(zmk_mouse_button_flags_t buttons);
int zmk_hid_mouse_buttons_release(zmk_mouse_button_flags_t buttons);
void zmk_hid_mouse_movement_set(int16_t x, int16_t y);
void zmk_hid_mouse_scroll_set(int8_t x, int8_t y);
void zmk_hid_mouse_movement_update(int16_t x, int16_t y);
void zmk_hid_mouse_scroll_update(int8_t x, int8_t y);
void zmk_hid_mouse_clear(void);

#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

struct zmk_hid_keyboard_report *zmk_hid_get_keyboard_report(void);
struct zmk_hid_consumer_report *zmk_hid_get_consumer_report(void);

#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
zmk_hid_boot_report_t *zmk_hid_get_boot_report();
#endif

#if IS_ENABLED(CONFIG_ZMK_POINTING)
struct zmk_hid_mouse_report *zmk_hid_get_mouse_report();
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

#if IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT)
struct zmk_hid_trackpad_report *zmk_hid_get_trackpad_report(void);
void zmk_hid_trackpad_clear(void);
#endif // IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT)
