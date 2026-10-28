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

plugin_configure   Configure the project and prepare matching Fedora source RPM headers
plugin_build       Build the plugin and tests
plugin_clean       Clean outputs and all cached Konsole SRPM/prepared trees
plugin_test        Run tests in isolated project-local settings
plugin_stage       Install into build/stage for inspection (no system changes)
plugin_rpm         Configure, build, and generate an RPM under build/packages
dev_build          Configure and build
run_konsole        Run installed Konsole with the build-tree plugin (--separate)

Extra arguments go to CMake/CTest/Konsole. CQ_BUILD_DIR, CQ_JOBS (4),
CQ_BUILD_TYPE (Debug), and CQ_SYSTEM_KONSOLE can be overridden.
No function installs system-wide. End users can use CMake directly.
HELP
}

plugin_configure() {
    cmake -S "$CQ_ROOT" -B "$CQ_BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$CQ_BUILD_TYPE" -DBUILD_TESTING=ON \
        -DCMAKE_INSTALL_PREFIX=/usr -DCQ_KONSOLE_EXECUTABLE="$CQ_SYSTEM_KONSOLE" "$@"
}
plugin_build() {
    CMAKE_BUILD_PARALLEL_LEVEL="$CQ_JOBS" cmake --build "$CQ_BUILD_DIR" --parallel "$CQ_JOBS" "$@"
}
plugin_clean() {
    plugin_build --target clean "$@"
}
plugin_test() {
    ctest --test-dir "$CQ_BUILD_DIR" --output-on-failure "$@"
}
plugin_stage() {
    DESTDIR="$CQ_BUILD_DIR/stage" cmake --install "$CQ_BUILD_DIR" "$@"
}
plugin_rpm() {
    plugin_configure -DCQ_BUILD_RPM=ON && plugin_build &&
        cpack --config "$CQ_BUILD_DIR/CPackConfig.cmake" -G RPM "$@"
}
dev_build() {
    plugin_configure && plugin_build
}
run_konsole() {
    QT_PLUGIN_PATH="$CQ_BUILD_DIR/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
        "$CQ_SYSTEM_KONSOLE" --separate "$@"
}
if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    if (( $# == 0 )); then dev_help; else "$@"; fi
fi
