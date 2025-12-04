# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

target_sources(app PRIVATE hogp_central.c)

target_sources_ifdef(CONFIG_ZMK_HOGP_MOUSE_OUTPUT app PRIVATE hogp_mouse.c)
target_sources_ifdef(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT app PRIVATE hogp_trackpad.c)

target_sources_ifdef(CONFIG_ZMK_SERIAL_CMD app PRIVATE serial_cmd.c)
