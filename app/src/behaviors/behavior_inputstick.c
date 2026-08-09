/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Select or clear an InputStick output target, mirroring how &bt BT_SEL /
 * BT_CLR work for BLE profiles:
 *
 *   &is IS_SEL 0    route output through the dongle bound to slot 0
 *   &is IS_CLR 0    forget slot 0's dongle so it re-binds on next sighting
 */

#define DT_DRV_COMPAT zmk_behavior_inputstick

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <drivers/behavior.h>

#include <dt-bindings/zmk/inputstick.h>

#include <zmk/behavior.h>
#include <zmk/endpoints.h>
#include <zmk/hogp/inputstick.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case IS_SEL: {
        int err = inputstick_select_slot(binding->param2);
        if (err) {
            return err;
        }
        /* Selecting the target also makes it the preferred transport, so
         * keystrokes actually go there rather than out USB/BLE. */
        return zmk_endpoints_select_transport(ZMK_TRANSPORT_INPUTSTICK);
    }

    case IS_CLR:
        return inputstick_clear_slot(binding->param2);

    default:
        LOG_ERR("Unknown InputStick command: %d", binding->param1);
    }

    return -ENOTSUP;
}

static int behavior_inputstick_init(const struct device *dev) { return 0; }

static const struct behavior_driver_api behavior_inputstick_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_inputstick_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_inputstick_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
