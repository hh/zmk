/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/init.h>
#include <zephyr/settings/settings.h>

#include <stdio.h>

#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/usb_hid.h>
#include <zmk/hog.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
#include <zmk/hogp/inputstick.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DEFAULT_TRANSPORT                                                                          \
    COND_CODE_1(IS_ENABLED(CONFIG_ZMK_BLE), (ZMK_TRANSPORT_BLE), (ZMK_TRANSPORT_USB))

static struct zmk_endpoint_instance current_instance = {};
static enum zmk_transport preferred_transport =
    ZMK_TRANSPORT_USB; /* Used if multiple endpoints are ready */

static void update_current_endpoint(void);

#if IS_ENABLED(CONFIG_SETTINGS)
static void endpoints_save_preferred_work(struct k_work *work) {
    settings_save_one("endpoints/preferred", &preferred_transport, sizeof(preferred_transport));
}

static struct k_work_delayable endpoints_save_work;
#endif

static int endpoints_save_preferred(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    return k_work_reschedule(&endpoints_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
#else
    return 0;
#endif
}

bool zmk_endpoint_instance_eq(struct zmk_endpoint_instance a, struct zmk_endpoint_instance b) {
    if (a.transport != b.transport) {
        return false;
    }

    switch (a.transport) {
    case ZMK_TRANSPORT_USB:
        return true;

    case ZMK_TRANSPORT_BLE:
        return a.ble.profile_index == b.ble.profile_index;

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK:
        return a.inputstick.slot == b.inputstick.slot;
#endif
    }

    LOG_ERR("Invalid transport %d", a.transport);
    return false;
}

int zmk_endpoint_instance_to_str(struct zmk_endpoint_instance endpoint, char *str, size_t len) {
    switch (endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        return snprintf(str, len, "USB");

    case ZMK_TRANSPORT_BLE:
        return snprintf(str, len, "BLE:%d", endpoint.ble.profile_index);

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK:
        return snprintf(str, len, "STICK:%d", endpoint.inputstick.slot);
#endif

    default:
        return snprintf(str, len, "Invalid");
    }
}

#define INSTANCE_INDEX_OFFSET_USB 0
#define INSTANCE_INDEX_OFFSET_BLE ZMK_ENDPOINT_USB_COUNT
#define INSTANCE_INDEX_OFFSET_INPUTSTICK (ZMK_ENDPOINT_USB_COUNT + ZMK_ENDPOINT_BLE_COUNT)

int zmk_endpoint_instance_to_index(struct zmk_endpoint_instance endpoint) {
    switch (endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        return INSTANCE_INDEX_OFFSET_USB;

    case ZMK_TRANSPORT_BLE:
        return INSTANCE_INDEX_OFFSET_BLE + endpoint.ble.profile_index;

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK:
        return INSTANCE_INDEX_OFFSET_INPUTSTICK + endpoint.inputstick.slot;
#endif
    }

    LOG_ERR("Invalid transport %d", endpoint.transport);
    return 0;
}

int zmk_endpoints_select_transport(enum zmk_transport transport) {
    LOG_DBG("Selected endpoint transport %d", transport);

    if (preferred_transport == transport) {
        /* Do NOT early-return. preferred_transport is persisted and restored at
         * boot, so after a reboot it already equals the requested transport and
         * returning here skips update_current_endpoint() entirely -- leaving
         * current_instance on whatever was selected at startup. Pressing the
         * target key then does nothing at all, while the indicator happily
         * shows the target. Recompute and get out. */
        update_current_endpoint();
        return 0;
    }

    preferred_transport = transport;

    endpoints_save_preferred();

    update_current_endpoint();

    return 0;
}

int zmk_endpoints_toggle_transport(void) {
    enum zmk_transport new_transport =
        (preferred_transport == ZMK_TRANSPORT_USB) ? ZMK_TRANSPORT_BLE : ZMK_TRANSPORT_USB;
    return zmk_endpoints_select_transport(new_transport);
}

struct zmk_endpoint_instance zmk_endpoints_selected(void) { return current_instance; }

enum zmk_transport zmk_endpoints_preferred_transport(void) { return preferred_transport; }

static int send_keyboard_report(void) {
    switch (current_instance.transport) {
    case ZMK_TRANSPORT_USB: {
#if IS_ENABLED(CONFIG_ZMK_USB)
        int err = zmk_usb_hid_send_keyboard_report();
        if (err) {
            LOG_ERR("FAILED TO SEND OVER USB: %d", err);
        }
        return err;
#else
        LOG_ERR("USB endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_USB) */
    }

    case ZMK_TRANSPORT_BLE: {
#if IS_ENABLED(CONFIG_ZMK_BLE)
        struct zmk_hid_keyboard_report *keyboard_report = zmk_hid_get_keyboard_report();
        int err = zmk_hog_send_keyboard_report(&keyboard_report->body);
        if (err) {
            LOG_ERR("FAILED TO SEND OVER HOG: %d", err);
        }
        return err;
#else
        LOG_ERR("BLE HOG endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_BLE) */
    }

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK: {
        struct zmk_hid_keyboard_report *keyboard_report = zmk_hid_get_keyboard_report();
#if IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_NKRO)
        /*
         * NKRO stores held keys as a BITMAP -- keys[i] bit j means usage code
         * i*8+j -- while the dongle speaks the 8-byte USB boot report, which
         * wants an actual list of keycodes. Forwarding the bitmap bytes
         * verbatim sends the dongle nonsense that is mostly zeros, i.e. "no
         * keys pressed", so a correctly connected dongle types nothing at all.
         *
         * (zmk_hid_get_boot_report() does this conversion already but is gated
         * behind CONFIG_ZMK_USB_BOOT, which we do not want to turn on just for
         * its side effects on the USB stack.)
         */
        uint8_t keys[6] = {0};
        size_t n = 0;
        for (size_t i = 0; i < sizeof(keyboard_report->body.keys) && n < ARRAY_SIZE(keys); i++) {
            uint8_t bits = keyboard_report->body.keys[i];
            while (bits && n < ARRAY_SIZE(keys)) {
                int bit = __builtin_ctz(bits);
                keys[n++] = (uint8_t)(i * 8 + bit);
                bits &= (uint8_t)(bits - 1);
            }
        }
        return inputstick_send_keys(keyboard_report->body.modifiers, keys, n);
#else
        /* HKRO already is the boot layout: modifiers, reserved, 6 keycodes. */
        return inputstick_send_keys(keyboard_report->body.modifiers, keyboard_report->body.keys,
                                    sizeof(keyboard_report->body.keys));
#endif
    }
#endif
    }

    LOG_ERR("Unhandled endpoint transport %d", current_instance.transport);
    return -ENOTSUP;
}

static int send_consumer_report(void) {
    switch (current_instance.transport) {
    case ZMK_TRANSPORT_USB: {
#if IS_ENABLED(CONFIG_ZMK_USB)
        int err = zmk_usb_hid_send_consumer_report();
        if (err) {
            LOG_ERR("FAILED TO SEND OVER USB: %d", err);
        }
        return err;
#else
        LOG_ERR("USB endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_USB) */
    }

    case ZMK_TRANSPORT_BLE: {
#if IS_ENABLED(CONFIG_ZMK_BLE)
        struct zmk_hid_consumer_report *consumer_report = zmk_hid_get_consumer_report();
        int err = zmk_hog_send_consumer_report(&consumer_report->body);
        if (err) {
            LOG_ERR("FAILED TO SEND OVER HOG: %d", err);
        }
        return err;
#else
        LOG_ERR("BLE HOG endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_BLE) */
    }

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK:
        /* HID_DATA_CONSUMER (0x22) exists in the protocol and the dongle
         * exposes a Consumer Control interface, but it is not implemented
         * yet -- media keys silently do nothing on this transport. */
        return -ENOTSUP;
#endif
    }

    LOG_ERR("Unhandled endpoint transport %d", current_instance.transport);
    return -ENOTSUP;
}

int zmk_endpoints_send_report(uint16_t usage_page) {

    LOG_DBG("usage page 0x%02X", usage_page);
    switch (usage_page) {
    case HID_USAGE_KEY:
        return send_keyboard_report();

    case HID_USAGE_CONSUMER:
        return send_consumer_report();
    }

    LOG_ERR("Unsupported usage page %d", usage_page);
    return -ENOTSUP;
}

#if IS_ENABLED(CONFIG_ZMK_POINTING)
int zmk_endpoints_send_mouse_report() {
    switch (current_instance.transport) {
    case ZMK_TRANSPORT_USB: {
#if IS_ENABLED(CONFIG_ZMK_USB)
        int err = zmk_usb_hid_send_mouse_report();
        if (err) {
            LOG_ERR("FAILED TO SEND OVER USB: %d", err);
        }
        return err;
#else
        LOG_ERR("USB endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_USB) */
    }

    case ZMK_TRANSPORT_BLE: {
#if IS_ENABLED(CONFIG_ZMK_BLE)
        struct zmk_hid_mouse_report *mouse_report = zmk_hid_get_mouse_report();
        int err = zmk_hog_send_mouse_report(&mouse_report->body);
        if (err) {
            LOG_ERR("FAILED TO SEND OVER HOG: %d", err);
        }
        return err;
#else
        LOG_ERR("BLE HOG endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_BLE) */
    }

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK: {
        struct zmk_hid_mouse_report *mouse_report = zmk_hid_get_mouse_report();
        return inputstick_send_mouse(mouse_report->body.buttons, mouse_report->body.d_x,
                                     mouse_report->body.d_y, mouse_report->body.d_scroll_y);
    }
#endif
    }

    LOG_ERR("Unhandled endpoint transport %d", current_instance.transport);
    return -ENOTSUP;
}
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

#if IS_ENABLED(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT) || IS_ENABLED(CONFIG_ZMK_HOGP_ITRACK_OUTPUT)
int zmk_endpoints_send_trackpad_report() {
    switch (current_instance.transport) {
    case ZMK_TRANSPORT_USB: {
#if IS_ENABLED(CONFIG_ZMK_USB)
        int err = zmk_usb_hid_send_trackpad_report();
        if (err) {
            LOG_ERR("FAILED TO SEND TRACKPAD OVER USB: %d", err);
        }
        return err;
#else
        LOG_ERR("USB endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_USB) */
    }

    case ZMK_TRANSPORT_BLE: {
#if IS_ENABLED(CONFIG_ZMK_BLE)
        struct zmk_hid_trackpad_report *report = zmk_hid_get_trackpad_report();
        int err = zmk_hog_send_trackpad_report(&report->body);
        if (err) {
            LOG_ERR("FAILED TO SEND TRACKPAD OVER HOG: %d", err);
        }
        return err;
#else
        LOG_ERR("BLE HOG endpoint is not supported");
        return -ENOTSUP;
#endif /* IS_ENABLED(CONFIG_ZMK_BLE) */
    }

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK:
        /* The dongle has no trackpad interface wired up here. Return quietly
         * rather than hitting the "Unhandled endpoint transport" error below on
         * every single trackpad report. */
        return -ENOTSUP;
#endif
    }

    LOG_ERR("Unhandled endpoint transport %d", current_instance.transport);
    return -ENOTSUP;
}
#endif // CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT || CONFIG_ZMK_HOGP_ITRACK_OUTPUT

#if IS_ENABLED(CONFIG_SETTINGS)

static int endpoints_handle_set(const char *name, size_t len, settings_read_cb read_cb,
                                void *cb_arg) {
    LOG_DBG("Setting endpoint value %s", name);

    if (settings_name_steq(name, "preferred", NULL)) {
        if (len != sizeof(enum zmk_transport)) {
            LOG_ERR("Invalid endpoint size (got %d expected %d)", len, sizeof(enum zmk_transport));
            return -EINVAL;
        }

        int err = read_cb(cb_arg, &preferred_transport, sizeof(enum zmk_transport));
        if (err <= 0) {
            LOG_ERR("Failed to read preferred endpoint from settings (err %d)", err);
            return err;
        }

        update_current_endpoint();
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(endpoints, "endpoints", NULL, endpoints_handle_set, NULL, NULL);

#endif /* IS_ENABLED(CONFIG_SETTINGS) */

static bool is_usb_ready(void) {
#if IS_ENABLED(CONFIG_ZMK_USB)
    return zmk_usb_is_hid_ready();
#else
    return false;
#endif
}

static bool is_ble_ready(void) {
#if IS_ENABLED(CONFIG_ZMK_BLE)
    return zmk_ble_active_profile_is_connected();
#else
    return false;
#endif
}

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
static bool is_inputstick_ready(void) { return inputstick_is_ready(); }
#endif

static enum zmk_transport get_selected_transport(void) {
#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    /* An explicitly chosen dongle wins over USB/BLE readiness -- the whole
     * point of picking a target is that it stays picked. If the dongle is not
     * connected yet we deliberately fall through to USB/BLE so the keyboard
     * keeps typing somewhere useful; the status LED fast-blinks meanwhile. */
    if (preferred_transport == ZMK_TRANSPORT_INPUTSTICK) {
        if (is_inputstick_ready()) {
            return ZMK_TRANSPORT_INPUTSTICK;
        }
        LOG_DBG("InputStick preferred but not ready, falling back");
    }
#endif

    if (is_ble_ready()) {
        if (is_usb_ready()) {
            /* Both ready: honour the preference -- but NOT if the preference is
             * a transport we already rejected above as unready. Returning
             * INPUTSTICK here would route reports to a dongle that is not
             * connected, and inputstick_send_keys() would drop every one of
             * them with -ENOTCONN. Keystrokes would vanish entirely rather
             * than falling back. */
            enum zmk_transport pref = preferred_transport;
#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
            if (pref == ZMK_TRANSPORT_INPUTSTICK) {
                pref = ZMK_TRANSPORT_USB;
            }
#endif
            LOG_DBG("Both endpoint transports are ready. Using %d", pref);
            return pref;
        }

        LOG_DBG("Only BLE is ready.");
        return ZMK_TRANSPORT_BLE;
    }

    if (is_usb_ready()) {
        LOG_DBG("Only USB is ready.");
        return ZMK_TRANSPORT_USB;
    }

    LOG_DBG("No endpoint transports are ready.");
    return DEFAULT_TRANSPORT;
}

static struct zmk_endpoint_instance get_selected_instance(void) {
    struct zmk_endpoint_instance instance = {.transport = get_selected_transport()};

    switch (instance.transport) {
#if IS_ENABLED(CONFIG_ZMK_BLE)
    case ZMK_TRANSPORT_BLE:
        instance.ble.profile_index = zmk_ble_active_profile_index();
        break;
#endif // IS_ENABLED(CONFIG_ZMK_BLE)

#if IS_ENABLED(CONFIG_ZMK_INPUTSTICK)
    case ZMK_TRANSPORT_INPUTSTICK:
        instance.inputstick.slot = inputstick_active_slot();
        break;
#endif

    default:
        // No extra data for this transport.
        break;
    }

    return instance;
}

static int zmk_endpoints_init(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_init_delayable(&endpoints_save_work, endpoints_save_preferred_work);
#endif

    current_instance = get_selected_instance();

    return 0;
}

void zmk_endpoints_clear_current(void) {
    zmk_hid_keyboard_clear();
    zmk_hid_consumer_clear();
#if IS_ENABLED(CONFIG_ZMK_POINTING)
    zmk_hid_mouse_clear();
#endif // IS_ENABLED(CONFIG_ZMK_POINTING)

    zmk_endpoints_send_report(HID_USAGE_KEY);
    zmk_endpoints_send_report(HID_USAGE_CONSUMER);
}

static void update_current_endpoint(void) {
    struct zmk_endpoint_instance new_instance = get_selected_instance();

    if (!zmk_endpoint_instance_eq(new_instance, current_instance)) {
        // Cancel all current keypresses so keys don't stay held on the old endpoint.
        zmk_endpoints_clear_current();

        current_instance = new_instance;

        char endpoint_str[ZMK_ENDPOINT_STR_LEN];
        zmk_endpoint_instance_to_str(current_instance, endpoint_str, sizeof(endpoint_str));
        LOG_INF("Endpoint changed: %s", endpoint_str);

        raise_zmk_endpoint_changed((struct zmk_endpoint_changed){.endpoint = current_instance});
    }
}

void zmk_endpoints_refresh(void) { update_current_endpoint(); }

static int endpoint_listener(const zmk_event_t *eh) {
    update_current_endpoint();
    return 0;
}

ZMK_LISTENER(endpoint_listener, endpoint_listener);
#if IS_ENABLED(CONFIG_ZMK_USB)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_usb_conn_state_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(endpoint_listener, zmk_ble_active_profile_changed);
#endif

SYS_INIT(zmk_endpoints_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
