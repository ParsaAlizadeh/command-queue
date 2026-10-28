# Command Queue for Konsole

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
execution works. They are never installed or sourced automatically. Closing the
panel returns focus to the active terminal. Ctrl+Q is reserved by the action;
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

In a terminal running the newly built plugin, source the matching file:

```bash
# Bash 4.4+
source /home/parsa/project/command-queue/plugin/shell/integration.bash
```

```zsh
# Zsh
source /home/parsa/project/command-queue/plugin/shell/integration.zsh
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
focus restoration, stale layout restoration, and splitting in both directions
with the panel focused. It uses Konsole's real XMLGUI resources, the offscreen
Qt platform, and project-local XDG directories. Split terminals run a temporary
sleep process rather than user shell startup files; normal user settings are
not changed. Additional tests cover independent sessions, background updates,
partial marker sequences, input forwarding, session cleanup, and interactive
shell hooks. Hook tests use temporary homes and bypass user startup files.
An end-to-end test uses real Bash sessions to check ordered background execution,
shell variable persistence, independent queues/drafts, editing, and deletion.
Uninstalled shells are explicitly skipped (Zsh is currently absent on the
development machine; Bash integration is tested).

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
