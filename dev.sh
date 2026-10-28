#!/usr/bin/env bash
# Source for functions/exports, or execute: ./dev.sh plugin_build
# Do not change the caller's shell options when sourced.

export CQ_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
export KONSOLE_SOURCE_DIR="$CQ_ROOT/vendor/konsole"
export KONSOLE_BINARY_DIR="$CQ_ROOT/build/konsole"
export CQ_PLUGIN_BUILD_DIR="$CQ_ROOT/build/plugin"
export CCACHE_DIR="$CQ_ROOT/build/ccache"
export CQ_JOBS="${CQ_JOBS:-4}"
export CQ_BUILD_TYPE="${CQ_BUILD_TYPE:-Debug}"
export CQ_SYSTEM_KONSOLE="${CQ_SYSTEM_KONSOLE:-/usr/bin/konsole}"

dev_help() {
    cat <<'EOF'
Usage: ./dev.sh FUNCTION [arguments...]
       source ./dev.sh  # expose the same functions and project exports

konsole_version      Print installed Konsole's version (no package-manager query)
vendor_init          Clone matching Konsole release if absent; otherwise sync it
vendor_sync          Fetch/select installed Konsole's release; refuse dirty switches
konsole_configure    Configure private Konsole; extra arguments go to CMake
konsole_build        Build private Konsole; extra arguments go to cmake --build
plugin_configure     Configure plugin against the private source/build trees
plugin_build         Build plugin; extra arguments go to cmake --build
plugin_test          Run plugin tests; extra arguments go to CTest
dev_build            Configure/build Konsole, then configure/build the plugin
run_konsole          Run private Konsole with our plugin, in a separate process
run_system_konsole   Run installed Konsole with our plugin, in a separate process

Run vendor_init before the first configure. Run vendor_sync after upgrading
installed Konsole, then dev_build. No function installs files system-wide.
CQ_JOBS=2 and CQ_BUILD_TYPE=Debug are defaults; override them in the environment.
CQ_SYSTEM_KONSOLE selects the installed executable (default /usr/bin/konsole).
EOF
}

konsole_version() {
    local output version
    output=$(QT_QPA_PLATFORM=offscreen "$CQ_SYSTEM_KONSOLE" --version) || return
    if [[ $output =~ (^|[[:space:]])konsole[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+)($|[[:space:]]) ]]; then
        version=${BASH_REMATCH[2]}
        printf '%s\n' "$version"
    else
        printf 'Cannot parse Konsole version from: %s\n' "$output" >&2
        return 1
    fi
}

vendor_init() {
    if [[ -e $KONSOLE_SOURCE_DIR ]]; then
        vendor_sync
        return
    fi
    local version
    version=$(konsole_version) || return
    mkdir -p -- "$CQ_ROOT/vendor" || return
    git clone --depth 1 --branch "v$version" \
        https://invent.kde.org/utilities/konsole.git "$KONSOLE_SOURCE_DIR"
}

vendor_sync() {
    if [[ ! -d $KONSOLE_SOURCE_DIR/.git ]]; then
        printf 'No vendor checkout; run vendor_init first.\n' >&2
        return 1
    fi
    local version tag current wanted changes
    version=$(konsole_version) || return
    tag="v$version"
    changes=$(git -C "$KONSOLE_SOURCE_DIR" status --porcelain) || return
    if [[ -n $changes ]]; then
        printf 'Vendor checkout has local changes; refusing to switch or fetch.\n' >&2
        return 1
    fi
    if ! git -C "$KONSOLE_SOURCE_DIR" rev-parse --verify "refs/tags/$tag^{commit}" >/dev/null 2>&1; then
        git -C "$KONSOLE_SOURCE_DIR" fetch --depth 1 origin \
            "refs/tags/$tag:refs/tags/$tag" || return
    fi
    current=$(git -C "$KONSOLE_SOURCE_DIR" rev-parse HEAD) || return
    wanted=$(git -C "$KONSOLE_SOURCE_DIR" rev-parse "refs/tags/$tag^{commit}") || return
    if [[ $current == "$wanted" ]]; then
        printf 'Vendor Konsole already matches installed Konsole %s.\n' "$version"
        return 0
    fi
    git -C "$KONSOLE_SOURCE_DIR" checkout --detach "$tag" || return
    printf 'Vendor now matches %s; run dev_build to refresh the build trees.\n' "$version"
}

konsole_configure() {
    if [[ ! -f $KONSOLE_SOURCE_DIR/CMakeLists.txt ]]; then
        printf 'Missing Konsole source; run vendor_init first.\n' >&2
        return 1
    fi
    cmake -S "$KONSOLE_SOURCE_DIR" -B "$KONSOLE_BINARY_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$CQ_BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$KONSOLE_BINARY_DIR/install" "$@"
}

konsole_build() {
    cmake --build "$KONSOLE_BINARY_DIR" --parallel "$CQ_JOBS" "$@"
}

plugin_configure() {
    cmake -S "$CQ_ROOT/plugin" -B "$CQ_PLUGIN_BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$CQ_BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$CQ_PLUGIN_BUILD_DIR/install" \
        -DKONSOLE_SOURCE_DIR="$KONSOLE_SOURCE_DIR" \
        -DKONSOLE_BINARY_DIR="$KONSOLE_BINARY_DIR" "$@"
}

plugin_build() {
    cmake --build "$CQ_PLUGIN_BUILD_DIR" --parallel "$CQ_JOBS" "$@"
}

plugin_test() {
    ctest --test-dir "$CQ_PLUGIN_BUILD_DIR" --output-on-failure "$@"
}

dev_build() {
    konsole_configure && konsole_build && plugin_configure && plugin_build
}

run_konsole() {
    QT_PLUGIN_PATH="$CQ_PLUGIN_BUILD_DIR/plugins:$KONSOLE_BINARY_DIR/bin${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
        "$KONSOLE_BINARY_DIR/bin/konsole" --separate "$@"
}

run_system_konsole() {
    QT_PLUGIN_PATH="$CQ_PLUGIN_BUILD_DIR/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
        "$CQ_SYSTEM_KONSOLE" --separate "$@"
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    if (( $# == 0 )); then
        dev_help
    else
        "$@"
    fi
fi
