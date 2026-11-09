# Fix12 — native Windows desktop integration

This is a cumulative overlay for the original VM archive. It includes every
previous overlay. Restart the VM, extract into the existing CraftRoot, overwrite
files, and run `deploy-vm.cmd`. Do not apply it to an unrelated KDE installation.

## Changes

* Native standard-caption overlays restore their owner and foreground stacking
  after activation. Background decorations do not become globally topmost.
  The scoped Breeze style repairs its native frame after HWND recreation or a
  native style change. Custom titlebars continue to be skipped.
* Konsole sets its user-close state before closing the PTY. Its reader stops
  before pipes and buffers are released; console shutdown runs outside the GUI
  thread. Exit delivery is queued once and obsolete callbacks cannot affect a
  subsequent session. Windows profile environment overrides retain PATH and
  SystemRoot. This is still real Konsole with Windows shells.
* KWin Peek Desktop no longer minimizes every Windows task. The incompatible
  Show Desktop applet is removed from new and existing Windows panels. Individual
  task buttons still activate/minimize their own window.
* Task icons and process paths are cached, hung-window icon queries time out
  after 30 ms instead of 500 ms, and periodic task updates name the changing
  roles instead of invalidating all metadata. Caption sweeps run every 1.5 s;
  foreground and move events update their affected captions directly.

## Direct System Settings controls

The nine system pages no longer launch `ms-settings:` URLs.

| Page | Implemented controls |
| --- | --- |
| Display | Monitor and resolution/refresh selection, driver test, 15-second confirmation, restore on timeout |
| Sound | Current default playback endpoint volume and mute |
| Network | Adapter enumeration, DNS servers/reset, automatic IPv4 DHCP; administrator launch entry |
| Bluetooth | Enumerate radios, incoming connections, discoverability |
| Language & Input | Choose among installed input layouts and set the default |
| Notifications | Install/authorize/open the existing relay and send a KDE test notification |
| Accounts | Change the current local account password using NetUserChangePassword |
| Power | Select a real power plan, set AC screen-off/sleep timeouts; restore previous values if applying fails |
| Updates | Explicit asynchronous Windows Update search and available software update titles |

Changes happen only when their buttons are pressed. Passwords are passed directly
to the native API, cleared afterward, and never written to command lines or logs.
Network changes can require elevation; use that page's administrator entry.
Closing Settings stops its own asynchronous PowerShell queries.

Bluetooth pairing, device output selection, display scaling, display-language
pack installation, Microsoft/domain account management, DC power timeouts, and
update installation/restart control are not implemented. The pages identify
these limits instead of providing a Windows Settings shortcut. Display previews
are not saved to the registry until confirmed; if the entire application crashes
during a preview, reboot restores the previously saved display configuration.

## Dolphin and archive defaults

The first patched VM session registers per-user Directory and Drive open verbs
for Dolphin and KDE MIME defaults. It does not replace Explorer's shell executable
or namespace-folder handlers. Changes run once per installation path; later user
choices are preserved. To apply them again, run `install-file-associations.cmd`.

The project-owned **Archive Manager** uses Breeze and KArchive. It lists archives
and extracts into a newly created folder, then opens that folder in Dolphin.
ZIP, 7z, TAR, tar.gz, tar.bz2, tar.xz and tar.zst depend on the available KArchive
codecs. Listing and extraction run in a separate cancellable process. Encrypted
archives, RAR, archive editing and PAX extended TAR headers are not supported.
PAX archives are rejected explicitly to avoid silently using the wrong filename.
UTF-8 ZIP flags and UTF-8 GNU TAR paths are decoded correctly on Windows.

Extraction checks paths before writing, rejects symlinks, traversal, absolute
paths, Windows device names and alternate data streams, and refuses existing
output files/reparse points. Limits are 100,000 entries, 64 directory levels and
64 GiB declared uncompressed content. These are extraction limits; some KArchive
formats can allocate memory while reading metadata/decompressing.
Cancelling extraction may leave its newly created partial output folder.

Windows ZIP defaults protected by **UserChoice** are preserved. In System Settings
→ About & Diagnostics choose **Choose protected archive default (Open With)…**,
select an archive, choose KDE Archive Manager, and select the system's always-use
option. Alternatively use the file's Open With menu. Registration supplies
fallbacks for `.zip`, `.7z`, `.tar`, `.tgz`, `.tbz2`, `.txz`; Windows may continue to
use an already protected application until you choose a new one.

Undo with `install-file-associations.cmd -Restore`. Registry originals are in
`%LOCALAPPDATA%\plasma-win-session\file-associations-backup.json`; KDE MIME
originals are in `mime-defaults-backup.ini` in that directory. Restore changes only
values still owned by this installer; it preserves subsequent user changes.

## Validation and VM acceptance

Builds cover KWindowSystem, plasma-workspace task manager, Konsole, KArchive and
native tools. Fresh SHA-256-verified official archives accept every canonical
patch with strict `git apply`; patched files match the compiled source.

Automated checks include repeated live ConPTY closes, real Konsole Session
close/recreation and GUI heartbeat, hidden native frame-style repair/HWND
recreation, offscreen application/titlebar checks, menu migration, and real
archive extraction of Unicode/empty files plus invalid-path/symlink rejection.
Host file associations and system settings are never changed by these checks.

VM acceptance is still required: activate/drag/maximize ordinary windows, close
and reopen Konsole tabs, click taskbar buttons and blank areas, verify Dolphin
folder opening, choose protected ZIP defaults, and apply the desired system
settings. Remote/administrator windows and custom captions have different limits
from ordinary same-user standard-caption windows.

The ConPTY shutdown order follows Microsoft's
[ClosePseudoConsole guidance](https://learn.microsoft.com/en-us/windows/console/closepseudoconsole).
