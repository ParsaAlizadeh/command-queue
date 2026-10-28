# Command Queue: toggle-only prototype

This stage provides only an empty bottom dock named **Command Queue**.
**Ctrl+Q** or **Plugins → Show Command Queue** toggles it. Closing it returns
focus to the active terminal. There is no command editor, queue, shell hook,
input interception, or command execution. Ctrl+Q is reserved by the action;
other terminal input retains Konsole's normal handling. The shortcut appears
in Konsole's shortcut configuration as `Show Command Queue`.

The dock starts hidden when created. Konsole may subsequently restore dock
layout from its normal window settings. It can be floated using Qt's standard
dock controls. Each Konsole main window owns its own dock and action.

## Build

From `/home/parsa/project/command-queue`:

```bash
./dev.sh plugin_configure
./dev.sh plugin_build
./dev.sh plugin_test
```

This configures a separate project; it does not add the plugin to Konsole's
source tree or rebuild Konsole. The Konsole libraries must already be built.
The test loads the compiled module through KDE's factory mechanism and uses
the real Konsole main window and terminal widget. It checks discovery,
metadata, shortcut toggling, menu action state, close-button handling, and
focus restoration. It uses the offscreen Qt platform and project-local XDG
directories, without launching a shell or changing normal user settings.

## Private headers and libraries

There is no `find_package(Konsole)` plugin SDK. Instead CMake uses two explicit
cache paths (the defaults are the sibling directories shown above):

* `KONSOLE_SOURCE_DIR`: original headers, such as `src/MainWindow.h` and
  `src/pluginsystem/IKonsolePlugin.h`.
* `KONSOLE_BINARY_DIR`: generated headers, such as `src/config-konsole.h`,
  `src/konsoleapp_export.h`, and `src/konsoleprivate_export.h`, and the compiled
  libraries in `bin/`.

The configure step reads the host build's `CMakeCache.txt`, checks that its
source directory matches, and reads `CMAKE_PROJECT_VERSION`. That version is
embedded in the plugin metadata. This is the host compatibility version, not
the plugin project's own feature version.

`find_library(... NO_DEFAULT_PATH)` locates only the libraries in the supplied
Konsole build, then imported CMake targets link the module to `konsoleapp` and
`konsoleprivate`. Normal `find_package` calls locate Fedora's Qt6 and KDE
Frameworks dependencies. The development module has a build RPATH pointing
at the private library directory; it is not a relocatable release package.

## Output and discovery

The build produces:

```text
build/plugin/plugins/konsoleplugins/konsole_commandqueue.so
```

This is a Qt/KDE shared plugin module, not an executable. Its JSON metadata
is embedded by Qt's meta-object compiler; no separate JSON installation is
needed. Point `QT_PLUGIN_PATH` at the directory **containing** `konsoleplugins`,
not at that subdirectory itself.

Launch a fresh private Konsole process from the project root:

```bash
./dev.sh run_konsole
```

`--separate` avoids forwarding the request to an already-running Konsole,
which would not inherit this environment. The second search directory also
exposes the private build's existing plugins. No installation is necessary.

Konsole's `PluginManager` calls `KPluginMetaData::findPlugins("konsoleplugins")`.
It filters metadata by the first five version characters (`26.08` here),
instantiates accepted plugins, and asks them to create widgets and Plugins
menu actions for each main window. There is no separate enable/disable
checkbox: discovery loads the plugin; the menu action shows/hides its dock.
Restart Konsole after rebuilding to load the new binary.

The same discovery method can be tried with Fedora's Konsole, currently also
26.08.1:

```bash
./dev.sh run_system_konsole
```

The automated test targets the private build, not Fedora's process. Matching
version metadata alone does not prove binary compatibility with downstream
patches. An updated repository Konsole may reject this module or have different
private-library dependencies; rebuild against a matching source/build before
using it there. Publishing/installing a relocatable plugin is a later stage.

Nothing is copied into `/usr`, `~/.local`, or Fedora's Qt plugin directories.
This prototype intentionally has no plugin installation rule. CMake's generic
KDE installation-prefix message does not mean an installation was performed.
