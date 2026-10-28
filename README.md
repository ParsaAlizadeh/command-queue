# Command Queue for Konsole

Plugin source lives in `plugin/`. The current prototype only toggles an empty
bottom panel. See [plugin/README.md](plugin/README.md) for its architecture and
discovery behavior.

## Development

`dev.sh` is the common entry point for interactive and automated development.
It can be executed from any working directory, or sourced into Bash to expose
the same functions and project-local environment variables.

```bash
./dev.sh konsole_version
./dev.sh vendor_init
./dev.sh konsole_configure
./dev.sh konsole_build
./dev.sh plugin_configure
./dev.sh plugin_build
./dev.sh plugin_test
./dev.sh run_konsole
```

`vendor_init` clones the release reported directly by `/usr/bin/konsole --version`.
`vendor_sync` selects the matching tag after an installed Konsole upgrade.
It refuses local vendor changes and never resets or cleans the checkout.
Missing release tags produce an error rather than falling back to another
version. The source version matches upstream; Fedora patches are not imported.

After initializing or syncing the vendor, `./dev.sh dev_build` performs all four
configure/build steps. It does not install anything. `run_system_konsole` is
available for checking the plugin against repository Konsole, subject to the
private ABI limitations described in the plugin documentation.

```bash
source ./dev.sh
plugin_build
plugin_test
run_konsole
```

`./dev.sh` with no arguments lists functions. Configure/build/test/run functions
forward additional arguments to their underlying commands. Override parallelism
and build type using `CQ_JOBS` and `CQ_BUILD_TYPE`; their defaults are `2` and
`Debug`. `CQ_SYSTEM_KONSOLE` overrides the installed executable used for version
detection and the system launch function. Version detection uses Qt's offscreen
platform so it also works without graphical display access.

Tracked files contain the plugin and development helpers. `vendor/`, `build/`,
and scratch `work/` are ignored. Konsole source is unmodified; generated headers,
libraries, plugin binaries, compiler cache, and isolated test state stay under
`build/`. Use `dev.sh` for development commands to keep paths and environment
consistent.
