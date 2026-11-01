# Command Queue developer guide

For a quick introduction and Fedora installation commands, see [README.md](README.md).
This guide covers behaviour, shell integration, architecture, build internals,
packaging, and development commands. Plugin source lives in `plugin/`.

## Panel behaviour

**Ctrl+Q** or **Plugins → Show Command Queue** toggles the bottom panel.
The list shows pending commands for the last focused terminal. Type a single-line
command in the bottom textbox and press Enter to queue it. Commands run in order
in that session, including while another tab or split is focused or the panel
is hidden. Executed commands leave the list; their output stays in the terminal.
The read-only debug textbox shows the session ID, readiness state and queue size.

Up/Down in the textbox selects pending commands for editing; Enter saves,
Escape cancels, and Down past the last item restores your draft. Double-clicking
a list item also edits it. Dispatch for that session is held while editing.
Select an item and click Delete to remove it. Each session retains its own draft
and edit state when switching terminals. Queues are in memory and discarded
when their session closes. There is no success condition yet.

Optional shell hooks below must be sourced in each shell before automatic
execution works. Installation ships the scripts but never sources them automatically. Closing
the panel returns focus to the active terminal. Ctrl+Q is reserved by the action;
other terminal input retains Konsole's normal handling. The shortcut appears
in Konsole's shortcut configuration as `Show Command Queue`.

The dock starts closed. Only the plugin's shortcut/menu toggle and the dock's
close button change its requested open/closed state. Internal Qt visibility
notifications and layout restoration do not toggle that state or cause the
plugin to move keyboard focus. The plugin uses its own checkable action rather
than deriving state from `QDockWidget::toggleViewAction()`.

Explicit toggles also refresh KDE's in-memory window-layout snapshot. Otherwise
XMLGUI can restore an older dock state while switching terminals, move focus,
and recursively rebuild menus/toolbars during a split. No Konsole source patch
is needed. Each main window owns a single dock which follows its active terminal.

## Session ownership and readiness

`activeViewChanged()` binds the panel to the controller's `Session`. There is
one `SessionTracker` per observed session, not per view or main window. Moving
focus to the panel keeps the last terminal selected. Switching tabs or split
panes changes the displayed session; background observers continue receiving
reports, and returning to a session shows its retained state. Trackers are
removed when their session is destroyed. Status updates neither open the panel
nor move focus. Each tracker owns a `SessionQueue`; dispatch is independent of
the displayed session.

The panel distinguishes unknown, prompt reported, input sent, running,
continuation input, read-only, and closed states. Without shell reports, queued
commands wait. Dispatch requires a prompt report, a running writable session,
the primary terminal screen, and no active queue edit. Input sent to the terminal
invalidates the current prompt report without intercepting or modifying bytes.

This prototype assumes no leftover input in the shell's editing buffer. A prompt
report dispatches immediately: there is no timer, buffer clearing, or readiness
handshake. Readiness is consumed before sending one command, so another command
waits for the next prompt report. Typed-ahead input is an accepted limitation
for this version. Exit codes and command success are not tracked yet.

### Optional shell reports

From the repository root, in a terminal running the plugin, source the matching file:

```bash
# Bash 4.4+
source ./plugin/shell/integration.bash
```

```zsh
# Zsh
source ./plugin/shell/integration.zsh
```

This changes only that shell session. No startup file is edited. Re-sourcing
does not append duplicate prompt markers. Source after prompt setup; frameworks
that subsequently replace PS1/PS2/PS0 can remove reports, requiring re-sourcing.
Start a fresh shell to discard the optional hooks.

The hooks append zero-width primary/continuation prompt markers. Bash uses PS0
to report command acceptance, preserving existing PS0, PROMPT_COMMAND, and DEBUG
traps. Zsh adds a preexec hook without replacing existing hooks. The tiny v1
protocol is `OSC 777 ; command-queue ; 1 ; EVENT BEL`, with `prompt`, `running`,
and `continuation` events. Konsole already parses OSC 777 and exposes its events
immediately through `osc777Received`; the plugin does not parse raw terminal
bytes or modify Konsole. Existing OSC 133 semantic integration alone does not
enable this custom observer. The reports are lifecycle hints, not a security
boundary or proof of an empty editor buffer.

## Build environment and workflow

This project targets the installed Fedora Konsole and its matching Fedora source
RPM. Install Konsole, CMake 3.24+, a C++17 compiler, `rpm-build`, and Konsole's build
dependencies (`sudo dnf builddep konsole`). The build uses `dnf5`/`dnf`, `rpm`,
`rpmbuild`, Fedora's KDE/CMake RPM macros, and Qt 6/KDE Frameworks 6
development packages. It does not install dependencies automatically.

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Ninja is optional for these commands. Normal builds compile only our plugin and
tests. **Konsole itself and its libraries are never compiled or installed.**

During configuration, CMake:

1. Queries RPM ownership of the installed Konsole executable, taking version,
   architecture and exact source-RPM identity from the installed RPM database.
   It does not execute `konsole --version` or select a repository's newest build.
2. Finds the installed versioned `libkonsoleapp` and `libkonsoleprivate` libraries
   and verifies that their source RPM and architecture match Konsole's.
3. Downloads that exact SRPM through `dnf download --srpm`, unless already cached,
   using only `fedora-source` and `updates-source`. If the command fails or
   produces no expected file, it clears project DNF metadata and retries once.
   It checks the cached RPM's identity and payload digests.
4. Extracts the spec and source files. Fedora's `%prep` applies its patches.
   Fedora's `%cmake_kf6` generates the real configuration and export headers;
   `%cmake_build` is overridden to `:`, so compilation does not run. CMake can
   still compile small compiler-detection probes during configuration.
5. Creates imported targets for the installed libraries. The subsequent build
   compiles our plugin against those headers and libraries.

The supported spec has `%prep` containing `%autosetup -p1`, and `%build`
containing only `%cmake_kf6` followed by `%cmake_build`. Changed recipe shapes
fail with an explanatory error rather than executing unexpected build steps.
There is no separately maintained Konsole header generation or copied Fedora
configure flag list. This follows the source and recipe; it does not recreate
the historical Fedora buildroot, compiler or RPM macro versions exactly.

The first configuration needs network access and the exact source package in
`fedora-source` or `updates-source` (enabled for this command only). Missing or
mismatched source packages cause a failure;
there is no fallback to another release or upstream Git checkout. DNF runs
noninteractively, without sudo or importing keys on the user's behalf. Repository
configuration and trusted keys for those Fedora repositories must already be
usable. Unrelated repositories are excluded, including their key-import prompts.

Source RPMs, extracted files, prepared sources, headers and logs live under:

```text
build/_deps/konsole-26.08.1-1.fc44.x86_64/
```

The exact directory follows the installed source package and architecture.
`download.log` records each download attempt and any metadata cleanup, including
exit statuses and command output; `configure.log` records header preparation.
DNF's cache, state and logs live under `build/dnf/`. Repeat configurations reuse the
cached SRPM and prepared source, then rerun Fedora's configure invocation without
reapplying patches. After upgrading Konsole, rerun configure, build and install;
a different SRPM gets its own directory. Unused old dependency directories can
be deleted.

`cmake --build build --target clean` (or `./dev.sh plugin_clean`) also removes
**all** Konsole source RPMs and extracted/prepared/configured trees under
`build/_deps/`, including old releases. The next configure downloads the required
SRPM again. A normal build after clean automatically reruns configuration to
regenerate missing headers. Compiler cache and DNF repository metadata are
retained; removing `build/` resets those too.

The plugin module is `build/plugins/konsoleplugins/konsole_commandqueue.so`.
Installation places it in KDE's Qt plugin directory, normally
`/usr/lib64/qt6/plugins/konsoleplugins/` with `/usr` as the prefix. Shell integration
installs to `<prefix>/share/konsole-command-queue/shell/`. Source the matching file
from your shell startup configuration, after prompt setup:

```bash
source /usr/share/konsole-command-queue/shell/integration.bash
```

Use `integration.zsh` for Zsh. Startup files are not edited automatically. Restart
Konsole to load a newly installed plugin. A custom prefix may require its Qt
plugin directory in `QT_PLUGIN_PATH`.

To inspect installation without modifying the system:

```bash
DESTDIR="$PWD/build/stage" cmake --install build
```

The staging tree contains the plugin, two integration files, build information
and license. Installed modules have no development RPATH. This uses Konsole's
private C++ interface; the matching Fedora source is preferable to unpatched
upstream headers, but private ABI compatibility still needs testing.

## Compiler cache

CMake optionally uses ccache when found (`CQ_USE_CCACHE=ON`, the default); it is
not required. Plugin compilation uses `<build-directory>/ccache`. The compiler
launchers explicitly set `CCACHE_DIR` and `CCACHE_TEMPDIR`, so invoking CMake
without `dev.sh` works identically. Fedora header-configuration subprocesses
inherit the same paths, including compiler probes using Fedora's ccache wrappers.
No cache files are installed. Use `-DCQ_USE_CCACHE=OFF` to disable caching,
including implicit compiler wrappers. Removing the build directory removes it.

## Versions

The plugin's own version is `project(... VERSION ...)` in the root CMake file,
currently **0.1.0**. Embedded JSON contains `CommandQueueVersion`, `KonsoleVersion`
and `KonsoleSourceRPM`; installed `build-info.json` records the same information.
The full SRPM identity records which Fedora patches and recipe supplied the
headers. There is no Git checkout or Git revision in the metadata.

`KPlugin.Version` contains the upstream Konsole version because the host uses
that field for compatibility filtering. It is not our feature version.
RPM packages use a combined version such as `0.1.0+konsole26.08.1` with release
`3.fc44`: our packaging revision (`CQ_RPM_RELEASE`, default 3) and RPM's dist tag.
Konsole's RPM release is not included. Revision 3 records the switch to MIT
licensing. When reusing an older build directory, pass `-DCQ_RPM_RELEASE=3` once
to update its cached revision.

## RPM packages

Enable CPack RPM generation with the system prefix:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCQ_BUILD_RPM=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --build build --target package
```

After building, `cpack --config build/CPackConfig.cmake -G RPM` also generates it.
The unsigned RPM appears under `build/packages/`; it is not installed. It includes
our files, not Konsole or compiler caches. RPM may add standard build-ID links.
DEB generation and cross-compilation are not supported.

The explicit Konsole package requirement is currently commented out in
`cmake/RPM.cmake`. If enabled, it is `konsole(x86-64) = 0:26.08.1`, accepting
any RPM release of that upstream version. Automatic shared-library requirements
remain enabled, including the full-version Konsole library names; removing the
explicit requirement does not necessarily unblock upgrades. CPack rechecks the
installed Konsole version/epoch/architecture before packaging; release-only changes
remain accepted. This package policy is separate from choosing the exact installed
SRPM when generating headers.

A newer upstream Konsole may be blocked by the plugin's dependencies. DNF may
fail or hold Konsole back, depending on solver options. `--allowerasing` permits
dependency-related removals, including the plugin, but can also propose others.
Distribution-only rebuilds are accepted on the assumption that they preserve the
private ABI. No automatic erasure or rebuild-on-upgrade mechanism is provided.

## Development helpers and tests

`dev.sh` is an optional wrapper; end users need only CMake.

```bash
./dev.sh dev_build
./dev.sh plugin_test
./dev.sh plugin_stage
./dev.sh plugin_rpm
./dev.sh run_konsole
```

Development defaults to Ninja, Debug, tests enabled, `/usr` as the intended
installation prefix, and four parallel jobs. No function installs system-wide.
`plugin_stage` uses `DESTDIR=build/stage`. `run_konsole` launches the installed
host with `--separate` and sets `QT_PLUGIN_PATH` for our build-tree module.

Override `CQ_BUILD_DIR`, `CQ_JOBS`, `CQ_BUILD_TYPE`, or `CQ_SYSTEM_KONSOLE` in the
environment. Configure/build/test/stage/run functions forward additional arguments.
`plugin_rpm` configures packaging and builds, then forwards its arguments to CPack.
`plugin_clean` removes build outputs and the complete Konsole source cache.
`./dev.sh` lists available functions.

Tests exercise the actual module against Fedora's installed Konsole libraries:
focus and splitting regressions, per-session readiness, editing/deletion, and
ordered background execution in Bash. They use isolated XDG settings and temporary
shell startup files; unavailable shells are skipped. Recipe tests reject direct
compilation or unexpected preparation/configuration stages. Use
`-DBUILD_TESTING=OFF` to omit tests and the Qt Test development dependency.

## Konsole API and plugin discovery

There is no public `find_package(Konsole)` SDK. The plugin includes original
private headers from Fedora's patched source and generated headers from its
configure step. It links to the installed `konsoleapp` and `konsoleprivate`
libraries while developing and after installation. Konsole is never compiled
or installed by us.

Konsole discovers `konsoleplugins/konsole_commandqueue.so` through Qt's plugin
search paths and loads it automatically. There is no separate enable checkbox;
the Plugins menu action controls panel visibility. Konsole compares the first
five characters of `KPlugin.Version` with its release version. Our build targets
the full release and separately embeds the plugin's own version and Fedora
source-RPM identity. Matching metadata is not proof of compatibility with downstream patches.
Restart Konsole after rebuilding or installing to load the new module.

## License

This project's original source is licensed under [MIT](LICENSE). Source SPDX
notices, plugin metadata, and RPM metadata declare `MIT`. Installation and RPM
packaging include the MIT text as the project's license file.

Konsole and other dependencies retain their own licenses. In particular,
Konsole's GPL-2.0-or-later headers and libraries are not relicensed by this
project's MIT license. Distributing the plugin for use with Konsole still requires
observing the applicable GPL terms for that integration. MIT is GPL-compatible;
see the [GNU guidance on plugin licensing](https://www.gnu.org/licenses/gpl-faq.en.html#GPLAndPlugins).
