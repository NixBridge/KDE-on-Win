# Start menu and personalization

Open the project-owned **System Settings** from Plasma.

* **Appearance:** Breeze Light/Dark, standard caption decorations and live
  application font size (8–20 pt). Font/color settings affect the project's Qt
  applications, not a universal Windows or Plasma desktop color scheme.
* **Panel:** height (24–96 logical pixels), top/bottom/left/right position,
  normal/auto-hide/dodge visibility, opacity and floating. **Apply to all panels**
  uses Plasma's actual scripting properties. **Reset panel defaults** restores
  the values shown in this page and applies them.
* **Desktop:** choose a local wallpaper for all Plasma desktops. Wallpaper-only
  changes preserve panel properties.
* **Start Menu:** 2–5 columns, 24–72 pixel icons, and named sidebar folders.
  Removing a sidebar folder removes only its menu entry, never its files.
* **About & Diagnostics:** open session logs, run runtime checks and open this guide.

Panel/wallpaper requests are asynchronous. If Plasma is not running, preferences
remain pending and the session helper retries on the next Plasma startup. The
helper clears successful requests, so later edits made through Plasma's own UI
are not overwritten at every login. Pending requests and other integration
preferences live in `%LOCALAPPDATA%\plasma-win-session\appearance.ini`.
Delivery status/errors are in `personalization.log` in that directory.

## Start menu behavior

Current-user and all-users program folders with the same name are merged.
Identical shortcuts are deduplicated while distinct argument strings are kept.
Folder names use Windows' localized display names, with Breeze folder/category
icons rendered as actual images. Built-in tools, including common Control Panel
applets and management consoles, use matching Breeze icons; unmatched third-party
applications retain their native icons before falling back to a generic icon.

Search filters the current folder without rescanning Windows on every keystroke.
Use Ctrl+F, arrow keys and Enter; Escape closes the menu, and Backspace in the grid
goes up a folder. Right-click an item to open it or reveal its location.
Custom sidebar folders can show ordinary documents as well as launchers.
The **Refresh** button discovers changed apps; returning from a folder preserves
the AppsFolder cache instead of losing Store applications.

Sidebar folders are stored in
`%LOCALAPPDATA%\plasma\startmenu-categories.json`; changes and menu sizing are
picked up within approximately one second. Filesystem folders and installed
applications are never moved to create these categories.

Panel auto-hide, transparency/floating visuals, mixed DPI, keyboard navigation and
live wallpaper changes still need VM acceptance testing. Windows service modules
and every KDE widget/theme extension are not implied by these controls.
