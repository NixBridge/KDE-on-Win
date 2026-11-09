# Standard Windows window decorations

`plasma-window-decorations.exe` starts with the Plasma session. It uses Breeze
widgets to cover standard native captions, without an application whitelist.
Windows retains its original caption styles and normal window behavior beneath
the overlay. Closing the decoration process leaves the original titlebars usable.
Supported Windows 11 DWM border colors are restored when a decoration is removed.

The manager handles title changes, move/resize events, foreground colors,
minimize, maximize/restore, close and the titlebar menu. Native caption dragging
is forwarded to the target, including Windows' normal move/resize behavior.
The caption overlay belongs to its target window rather than being globally
topmost. Geometry is positioned in physical screen coordinates for mixed DPI.

Client-extended/self-drawn captions, borderless/fullscreen windows, tool windows,
cloaked windows, other users' windows and higher-integrity windows are skipped.
Classification uses native styles and client/caption geometry; unusual custom
non-client implementations still need VM testing. The Windows 11 Snap Layout
hover menu over the original maximize button is not reproduced by the overlay.

This changes standard titlebars and supported borders. Arbitrary application
content cannot be redrawn in Breeze through an external decoration process.
The project's Qt applications use Breeze for their own widgets as well.

Open **System Settings → Appearance** to choose Breeze Light/Dark or disable
global decorations. These preferences apply to this project's applications and
caption overlays and are stored in
`%LOCALAPPDATA%\plasma-win-session\appearance.ini`. They do not apply a global
Windows widget theme or change Plasma's separate desktop color scheme.

The self-test creates two hidden, test-owned native HWNDs: a standard caption
and a client-drawn caption retaining WS_CAPTION. It verifies the former is
accepted and the latter rejected, without decorating any host windows.
Interactive positioning, snapping, modal dialogs and mixed-DPI behavior require
VM acceptance testing.

# Settings and text editor replacements

The Plasma menu provides project-owned **System Settings** and **Text Editor**
applications, with Breeze windows and icons. These are not ports of official
KDE System Settings or KWrite. Plain Notepad/Control Panel shortcuts and the
corresponding Windows app entries launch the replacements from this menu;
shortcuts with custom arguments retain their original behavior.

System Settings provides live light/dark and decoration preferences. Hardware,
account and operating-system pages open their real Windows settings pages,
with an explicit **Open Windows Settings** button rather than dummy controls.

Text Editor supports multiple file windows, unsaved-change prompts, editing,
undo/redo, search and atomic save. UTF-8 and BOM-marked UTF-16 LE/BE are supported;
BOMs and uniform CRLF/LF formats are retained. Unsupported/binary input is
rejected instead of silently converted. Mixed newline styles are normalized.
Files over 64 MiB are rejected. Windows default file associations are unchanged.

Direct entry points: `start-system-settings.cmd`, `start-text-editor.cmd` and
`start-window-decorations.cmd`. Existing panels gain settings/editor pins once,
preserving later user unpinning.

Fix13 covers the full physical caption width, handles native/Qt maximize-state differences and converts drag coordinates for the target DPI context. See [FIX13.md](FIX13.md) for validation and remaining VM acceptance.

Fix14 disables DWM transitions on attached standard-caption windows because the owner-only animation excludes the separate KDE caption. Minimized targets remain tracked. Detaching restores the default enabled transition state; the write-only DWM attribute prevents recovering another tool's previous override. See [FIX14.md](FIX14.md).
