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
execution works. Installation ships the scripts but never sources them automatically. Closing the
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

## Build, installation, and compatibility

See the [project README](../README.md) for the standard CMake workflow,
dependencies, compiler cache policy, installation paths, and versioning.
The root CMake project prepares headers from the matching Fedora source RPM;
`plugin/` contains the plugin sources and is no longer a standalone CMake project.

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
