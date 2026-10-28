# Command Queue for Konsole

Queue commands in a separate panel while your terminal keeps running normally.
Each tab or split has its own queue, which continues running in the background.
Press **Ctrl+Q** to show or hide the panel.

## Requirements

**This project is intended only for Fedora**, using Fedora's installed Konsole
and its matching source RPM. The exact source RPM must be available from your
enabled repositories. Network access is needed for the first configure.

Install the tools and Konsole's build dependencies. They are needed to configure
its headers, even though Konsole itself is not compiled:

```bash
sudo dnf install konsole cmake gcc-c++ make rpm-build dnf5-plugins
sudo dnf builddep konsole
```

## Build and install

From the repository root:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_TESTING=OFF -DCQ_BUILD_RPM=ON
cmake --build build --parallel 2
cpack --config build/CPackConfig.cmake -G RPM \
  -D CPACK_RPM_FILE_NAME=konsole-command-queue.rpm
sudo dnf install ./build/packages/konsole-command-queue.rpm
```

Restart Konsole after installation.

## Enable shell integration

In each terminal where you want to use the queue, run the command for your shell:

```bash
# Bash
source /usr/share/konsole-command-queue/shell/integration.bash
```

```zsh
# Zsh
source /usr/share/konsole-command-queue/shell/integration.zsh
```

For future terminals, add the matching line to `~/.bashrc` or `~/.zshrc` after
prompt setup. Installation does not edit these files.

## Use

- **Ctrl+Q:** open or close the queue.
- Type a single-line command and press **Enter** to queue it.
- **Up/Down** or double-click: edit a queued command; **Enter** saves, **Escape** cancels.
- Select a command and click **Delete** to remove it.

Queues are kept only for the current session. This is experimental: keep the
terminal input line empty while waiting for queued commands to run. Rebuild the
plugin after upgrading Konsole.

For architecture, testing, packaging, and development commands, see
[README_DEV.md](README_DEV.md).

## Uninstall

```bash
sudo dnf remove konsole-command-queue
```

Remove any integration line you added to `~/.bashrc` or `~/.zshrc`, then restart
Konsole.

## License

[MIT](LICENSE).
