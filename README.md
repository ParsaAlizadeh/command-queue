# Command Queue for Konsole

A Konsole plugin with a separate command queue for each terminal session.
**Ctrl+Q** opens or closes the panel. Queues continue running in background
sessions. See [plugin/README.md](plugin/README.md) for usage and shell integration.

## Build and install

Install Git, a C++17 compiler, CMake 3.24+, and the development dependencies for
the matching Konsole release (Qt 6, KDE Frameworks 6, ICU, libssh and the other
dependencies checked by Konsole's CMake). On Fedora, enabling source repositories
and running `sudo dnf builddep konsole` supplies Konsole's build dependencies;
Git and CMake must also be available. Other distributions need their equivalent
build-dependency setup. CMake reports missing dependencies; it does not install
them. Ninja is optional for the commands below.

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The first configure needs network access to KDE's Git repository. CMake reads
`konsole --version` directly, fetches the exact `vX.Y.Z` release tag, and configures
an isolated Konsole build. The normal build compiles its required libraries and
our plugin. Dependency source and generated files stay under `build/_deps/`.
There is no manual vendor checkout or separate configure/build procedure.
Konsole itself, its libraries, and its other plugins are **not installed**.

For explicit targeting, including future packaging environments where Konsole
cannot run, pass `-DCQ_KONSOLE_VERSION=26.08.1`. Otherwise each configure detects
the installed version again. After upgrading Konsole, rerun configure and build,
then reinstall the plugin. Different releases use separate dependency directories.
The resolved Git commit is recorded in metadata and `build-info.json`. Release
tags select an exact release, but are not a cryptographic guarantee against an
upstream tag being moved. Dependencies/toolchains must also match for reproducible
binaries. Advanced/offline builds may supply an existing checkout with CMake's
standard `FETCHCONTENT_SOURCE_DIR_KONSOLE_26_08_1` override.

The module installs to KDE's Qt plugin directory under `konsoleplugins/`;
CMake prints the destination during installation. On Fedora with `/usr` as the
prefix this is `/usr/lib64/qt6/plugins/konsoleplugins/`. Debian-family layouts
can differ; KDEInstallDirs determines the platform layout. Restart Konsole to
load it. A custom prefix may need its Qt plugin directory in `QT_PLUGIN_PATH`.

Shell integration installs to
`<prefix>/share/konsole-command-queue/shell/`. Source the matching file from your
shell startup configuration, after prompt setup, for example:

```bash
source /usr/share/konsole-command-queue/shell/integration.bash
```

Use `integration.zsh` for Zsh. Installation does not edit startup files or enable
hooks automatically.

To inspect installation without modifying the system:

```bash
DESTDIR="$PWD/build/stage" cmake --install build
```

Only the plugin, the two shell integration files, build information, and the
license should appear in that staging tree. Installed modules have no development
RPATH and resolve Konsole's versioned libraries from the system. An upstream release match
cannot guarantee compatibility with distribution patches: rebuild and test for
each target distribution. No public/stable Konsole plugin ABI is available.

## Compiler cache

CMake optionally uses ccache when found (`CQ_USE_CCACHE=ON`, the default); it is
not a required dependency. All compilation uses `<build-directory>/ccache`,
including the isolated Konsole build and configurations made without `dev.sh`.
The compiler launchers set `CCACHE_DIR` and `CCACHE_TEMPDIR` explicitly, overriding
inherited locations. Configure subprocesses receive the same locations, covering
Fedora's compiler symlinks to ccache. Nothing relies on an exported shell variable
surviving from configure to build. No cache files are installed.

Use `-DCQ_USE_CCACHE=OFF` to disable it, including implicit compiler wrappers.
Removing the build directory removes the cache as well. `CQ_KONSOLE_JOBS` controls
parallelism in the isolated dependency build (default 2); the outer build's
`--parallel` setting controls plugin compilation.

## Versions

The plugin's own version is `project(... VERSION ...)` in the root CMake file,
currently **0.1.0**. Target Konsole version and fetched Git commit are separate.
Embedded JSON contains `CommandQueueVersion`, `KonsoleVersion`, and
`KonsoleRevision`; the installed `build-info.json` records the same information.
`KPlugin.Version` must contain the Konsole version because the host uses that
field for compatibility filtering. It is not our feature version.

RPM packages use a combined version such as `0.1.0+konsole26.08.1`.
Their release, for example `1.konsole0.1.fc44`, combines our packaging revision
(`CQ_RPM_RELEASE`, default 1) with the Konsole RPM epoch and release. A distribution
rebuild of Konsole therefore also changes the plugin package identity, even when
the upstream version stays the same. Plugin functionality changes still bump the
project's own version. The compatibility metadata is separate from RPM dependencies.

## RPM packages

RPM generation is optional and requires `rpm`, `rpmbuild` (Fedora's `rpm-build`
package), and an installed, RPM-managed Konsole. Only native RPM builds are
supported; DEB generation is not configured. Normal source builds do not need
RPM tools. Configure packaging with the system prefix:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCQ_BUILD_RPM=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --build build --target package
```

After building, `cpack --config build/CPackConfig.cmake -G RPM` can also generate
the package. Output is under `build/packages/`; packaging does not install it.
The RPM contains the plugin, shell integration scripts, build information, and
license. RPM may additionally generate standard build-ID links. Neither Konsole
itself nor compiler caches are bundled. Shell activation remains explicit.

CMake queries the **installed RPM owning the selected Konsole executable** to
obtain its name, epoch, version, release and architecture. For example:
`konsole(x86-64) = 0:26.08.1-1.fc44`. This exact dependency is added alongside
RPM's automatic library requirements. `konsole --version` still selects the
upstream checkout; packaging fails if that version differs from the installed
RPM. CPack checks the installed package again before packaging and refuses a
stale configuration if Konsole has changed. Reconfigure and rebuild after an
upgrade. The generated RPM is unsigned.

The exact dependency can prevent a Konsole upgrade while this plugin is
installed. Depending on DNF's solver options, it may fail or retain the older
Konsole package. `--allowerasing` allows dependency-related removals, including
the plugin when needed; it may also propose removing other packages. Review the
transaction. No automatic removal or upgrade workaround is implemented.

## Development

`dev.sh` is an optional convenience wrapper. It can be executed or sourced;
users building/installing the plugin do not need it.

```bash
./dev.sh dev_build
./dev.sh plugin_test
./dev.sh plugin_stage
./dev.sh plugin_rpm
./dev.sh run_konsole
./dev.sh run_system_konsole
```

Development defaults to Ninja, Debug, tests enabled, `/usr` as the intended
installation prefix, and four parallel jobs. No development function installs
system-wide. `plugin_stage` uses `DESTDIR=build/stage`. `run_konsole` builds the
optional `konsole_host` target and launches that private host with the plugin;
`run_system_konsole` uses the installed host. `plugin_rpm` enables RPM packaging,
builds the project, and invokes CPack; additional arguments go to CPack. Both use `--separate` and set
`QT_PLUGIN_PATH` for the build-tree plugin. The binary is
`build/plugins/konsoleplugins/konsole_commandqueue.so`.

Override `CQ_BUILD_DIR`, `CQ_JOBS`, `CQ_BUILD_TYPE`, or `CQ_SYSTEM_KONSOLE` in the
environment. Configure/build/test/stage/run functions accept additional arguments
for the underlying commands. `./dev.sh` lists available functions.

The tests load the actual compiled plugin and exercise the real Konsole main
window, focus and splitting regressions, per-session readiness, editing/deletion,
and ordered background execution in Bash. They use isolated XDG settings and
temporary shell startup files. Unavailable shells are skipped. Use
`-DBUILD_TESTING=OFF` when tests and the Qt Test development dependency are unwanted.
