#!/usr/bin/env bash
# Source for development functions, or execute: ./dev.sh FUNCTION [arguments...]
export CQ_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
export CQ_BUILD_DIR="${CQ_BUILD_DIR:-$CQ_ROOT/build}"
export CQ_JOBS="${CQ_JOBS:-4}"
export CQ_BUILD_TYPE="${CQ_BUILD_TYPE:-Debug}"
export CQ_SYSTEM_KONSOLE="${CQ_SYSTEM_KONSOLE:-/usr/bin/konsole}"

dev_help() {
    cat <<'HELP'
Usage: ./dev.sh FUNCTION [arguments...]
       source ./dev.sh

plugin_configure   Configure the project and fetch matching Konsole sources
plugin_build       Build required Konsole libraries and the plugin
plugin_test        Run tests in isolated project-local settings
plugin_stage       Install into build/stage for inspection (no system changes)
dev_build          Configure and build
run_konsole        Build/run the private host with the plugin (--separate)
run_system_konsole Run the installed host with the build-tree plugin (--separate)

Extra arguments go to CMake/CTest/Konsole. CQ_BUILD_DIR, CQ_JOBS (4),
CQ_BUILD_TYPE (Debug), and CQ_SYSTEM_KONSOLE can be overridden.
No function installs system-wide. End users can use CMake directly.
HELP
}

plugin_configure() {
    cmake -S "$CQ_ROOT" -B "$CQ_BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$CQ_BUILD_TYPE" -DBUILD_TESTING=ON -DCQ_KONSOLE_JOBS="$CQ_JOBS" \
        -DCMAKE_INSTALL_PREFIX=/usr -DCQ_KONSOLE_EXECUTABLE="$CQ_SYSTEM_KONSOLE" "$@"
}
plugin_build() {
    CMAKE_BUILD_PARALLEL_LEVEL="$CQ_JOBS" cmake --build "$CQ_BUILD_DIR" --parallel "$CQ_JOBS" "$@"
}
plugin_test() {
    ctest --test-dir "$CQ_BUILD_DIR" --output-on-failure "$@"
}
plugin_stage() {
    DESTDIR="$CQ_BUILD_DIR/stage" cmake --install "$CQ_BUILD_DIR" "$@"
}
dev_build() {
    plugin_configure && plugin_build
}
run_konsole() {
    local KONSOLE_SOURCE_DIR KONSOLE_BINARY_DIR
    source "$CQ_BUILD_DIR/konsole-paths.sh" || return
    plugin_build --target konsole_host || return
    QT_PLUGIN_PATH="$CQ_BUILD_DIR/plugins:$KONSOLE_BINARY_DIR/bin${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
        "$KONSOLE_BINARY_DIR/bin/konsole" --separate "$@"
}
run_system_konsole() {
    QT_PLUGIN_PATH="$CQ_BUILD_DIR/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
        "$CQ_SYSTEM_KONSOLE" --separate "$@"
}
if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    if (( $# == 0 )); then dev_help; else "$@"; fi
fi
