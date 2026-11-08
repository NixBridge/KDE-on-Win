# Windows Plasma port status

This is an experimental native Windows x64 Plasma port using Win32/DWM, not a full Linux KDE stack. The VM has displayed the Plasma desktop, panel, task manager, tray and launcher. Feature fixes still require VM acceptance.

## Versions and build layout

Qt 6.11.1, KDE Frameworks 6.28.0, Plasma 6.7.4 and Dolphin/KIO Extras 26.04.3 are pinned in `manifests/` and `upstream/plasma-windows/upstream-versions.json`. MSVC 2022 v143 builds against an NTFS Craft root at `D:\KDE\CraftRoot`.

The source tree separates native modules (`native/`), build/verification utilities (`tools/`), VM launchers (`runtime/`), pins and package manifests (`manifests/`), D-Bus interface generation (`interfaces/`), and the vendored upstream source/patches (`upstream/plasma-windows/`). Historical bootstrap and patch-repair helpers are isolated in `tools/maintenance/`.

Run `build-port.ps1` after initializing the pinned Craft/blueprint environment. `package-port.ps1` makes a full runtime archive. `python tools/make-cumulative-patch.py --version VERSION` creates an overlay containing every fix required on top of the original VM package. Output goes into ignored `dist/`, `logs/` and `work/`; runtime binaries and local VM files are not committed.

## Implemented integration

- Windows Qt/QML paths and packaged interface metadata; no KWin compositor is run.
- Windows native window backend, tray bridge, power/session actions, volume and input-method applets.
- Desktop KIO protocol and notifier, notification Quick Charts dependency, activity SQLite backend.
- Windows shortcuts launch through their original local path, preserving arguments and working directory; drive letters are not interpreted as URL protocols.
- Breeze raster icons for the desktop and launcher. Windows built-in programs use functionally equivalent icons only after confirming that a candidate exists in the bundled theme.
- Opaque Windows context menus with explicit topmost flags before native surface creation. Other platforms retain upstream behavior.
- Panel-only stacking recovery when an Explorer taskbar overtakes the panel; the desktop wallpaper remains below applications.

## Verification

Native MSVC builds, strict source-patch application and patch format checks, recursive x64 PE import checks, ZIP CRC checks, a real FolderModel shortcut launch (arguments and working directory), Breeze icon rendering and theme-resource lookup have passed. An offscreen FolderModel context menu probe checks visibility, opaque background, topmost flags and painted output.

These checks do not prove real Windows focus/z-order behavior, application compatibility or long-term stability. Recheck right-click menus, notifications, taskbar stacking, application launch, Chinese input and multiple monitors in the VM.

## VM use

1. Extract the original runtime to `D:\KDE\CraftRoot`, or `C:\KDE\CraftRoot` with D: free for SUBST.
2. Restart the VM before applying a cumulative overlay, then overwrite files in the same CraftRoot. Every overlay works from a snapshot without earlier patches.
3. Run `deploy-vm.cmd` without arguments. Use `run-runtime-checks.cmd` for supporting diagnostics.
4. If the existing panel is transparent, run `apply-panel-opacity.cmd` while Plasma is running.
5. Test Explorer replacement only after the temporary session works. `deploy-vm.cmd install` and `restore` manage the current user's login shell.

Logs include `%TEMP%\plasmashell-debug.log` and `runtime-checks.txt`. Host builds and headless probes do not replace the host's login shell.

## Known limits and licensing

Global shortcut service integration remains incomplete. Dolphin has no Baloo full-disk indexing or PackageKit installation integration. Linux-specific services and all KDE applications are not included.

The root LICENSE contains an explicit AI-generation/assistance disclosure followed by the unmodified GPL v3 text. Independently authored integration code uses GPL-3.0-or-later; vendored source and patches retain their original per-file licenses. See AI_NOTICE.md and THIRD_PARTY.md.

## Default taskbar launcher migration

Windows panels explicitly pin the packaged Konsole, Dolphin and project-owned Process Monitor, Text Editor and System Settings launchers. At session startup, a background D-Bus migration waits for task-manager widgets, removes unavailable Discover/System Settings default pins, and preserves other user pins. Installed desktop tools are added once, preserving later user unpinning. An already running session can invoke repair-taskbar-pins.cmd. The VM confirmed that the preceding opaque context-menu fix works.

## Native desktop tools

Konsole 26.04.3 builds with its Windows ConPTY backend. The scoped launcher enables
Breeze widgets and custom main-window chrome for Konsole, Dolphin and the new
project-owned process monitor. Headless backend/UI probes pass; interactive VM
acceptance remains pending. See [DESKTOP_TOOLS.md](DESKTOP_TOOLS.md) for scope and use.

The session also starts standard-caption decorations without an application
whitelist; custom and client-extended captions are skipped. Text Editor and System
Settings are project-owned replacements, with Windows OS settings delegated to
their real pages. See [WINDOW_DECORATIONS.md](WINDOW_DECORATIONS.md).

Tray balloons use Plasma's notification service. Modern toast mirroring uses a
separate package with Windows notification-listener permission; see
[NOTIFICATIONS.md](NOTIFICATIONS.md). SDK manifest validation and isolated relay
tests pass; real registration, permission and toast delivery need VM acceptance.
