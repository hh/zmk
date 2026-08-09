# Copyright (c) 2024 The ZMK Contributors
# SPDX-License-Identifier: MIT

# Generate build version header at build time
set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/zmk_build_version.h)
add_custom_command(
    OUTPUT ${VERSION_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DCONFIG_GIT_DIR=${CMAKE_SOURCE_DIR}/../..
        -DZMK_GIT_DIR=${CMAKE_SOURCE_DIR}/..
        -DOUTPUT_FILE=${VERSION_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating build version header"
    VERBATIM
)
add_custom_target(zmk_build_version DEPENDS ${VERSION_HEADER})

# Make the generated header available
target_include_directories(app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(app zmk_build_version)

target_sources_ifdef(CONFIG_ZMK_HOGP app PRIVATE hogp_central.c)
target_sources_ifdef(CONFIG_ZMK_INPUTSTICK app PRIVATE inputstick.c)

target_sources_ifdef(CONFIG_ZMK_HOGP_MOUSE_OUTPUT app PRIVATE hogp_mouse.c)
target_sources_ifdef(CONFIG_ZMK_HOGP_TRACKPAD_OUTPUT app PRIVATE hogp_trackpad.c)
target_sources_ifdef(CONFIG_ZMK_HOGP_ITRACK_OUTPUT app PRIVATE hogp_itrack.c)

target_sources_ifdef(CONFIG_ZMK_SERIAL_CMD app PRIVATE serial_cmd.c)
