# Generate build version header from git info.
# Called at build time by CMake.
#
# This firmware is built from TWO repositories and you need both to identify
# an image:
#   CONFIG_GIT_DIR - the Adv360-Pro-ZMK config repo (keymap, conf, build scripts)
#   ZMK_GIT_DIR    - the vendored zmk tree, where the actual firmware code lives
#                    (HOGP central, InputStick client, ZMK itself)
#
# Reporting only the config repo -- as this script used to -- names a commit
# that may contain none of the code that is actually running. Both are
# reported, each with its own dirty flag.

# Read branch, short commit and dirty flag out of one repo.
# Sets ${prefix}_BRANCH, ${prefix}_COMMIT, ${prefix}_DIRTY in the caller.
function(git_describe_repo dir prefix)
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${dir}
        OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT commit)
        set(commit "unknown")
    endif()

    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${dir}
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT branch)
        set(branch "unknown")
    endif()

    # --ignore-submodules=none so uncommitted changes inside the vendored zmk
    # tree still mark the config repo dirty; the whole point of tonight's
    # reproducibility work was that those edits were invisible for months.
    execute_process(
        COMMAND git status --porcelain --ignore-submodules=none
        WORKING_DIRECTORY ${dir}
        OUTPUT_VARIABLE status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(status)
        set(dirty "-dirty")
    else()
        set(dirty "")
    endif()

    set(${prefix}_BRANCH "${branch}" PARENT_SCOPE)
    set(${prefix}_COMMIT "${commit}" PARENT_SCOPE)
    set(${prefix}_DIRTY "${dirty}" PARENT_SCOPE)
endfunction()

git_describe_repo("${CONFIG_GIT_DIR}" CFG)
git_describe_repo("${ZMK_GIT_DIR}" ZMK)

execute_process(
    COMMAND git describe --tags --exact-match HEAD
    WORKING_DIRECTORY ${CONFIG_GIT_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_TAG)
    set(GIT_TAG "")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S" UTC)

# One line that identifies the image completely, e.g.
#   dec19-game-studio/0df2c40a zmk:34f3c9b8
set(VERSION_STRING "${CFG_BRANCH}/${CFG_COMMIT}${CFG_DIRTY} zmk:${ZMK_COMMIT}${ZMK_DIRTY}")
if(GIT_TAG)
    set(VERSION_STRING "${GIT_TAG} (${VERSION_STRING})")
endif()

file(WRITE ${OUTPUT_FILE}
"/* Auto-generated build version - DO NOT EDIT */
#ifndef ZMK_BUILD_VERSION_H
#define ZMK_BUILD_VERSION_H

#define ZMK_BUILD_COMMIT \"${CFG_COMMIT}${CFG_DIRTY}\"
#define ZMK_BUILD_TAG \"${GIT_TAG}\"
#define ZMK_BUILD_BRANCH \"${CFG_BRANCH}\"
#define ZMK_BUILD_ZMK_COMMIT \"${ZMK_COMMIT}${ZMK_DIRTY}\"
#define ZMK_BUILD_ZMK_BRANCH \"${ZMK_BRANCH}\"
#define ZMK_BUILD_TIME \"${BUILD_TIME}\"
#define ZMK_BUILD_VERSION \"${VERSION_STRING}\"

#endif /* ZMK_BUILD_VERSION_H */
")

message(STATUS "Generated build version: ${VERSION_STRING}")
