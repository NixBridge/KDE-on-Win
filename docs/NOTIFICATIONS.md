# Windows notifications in Plasma

Traditional Shell_NotifyIcon balloon messages received by `trayhost` are forwarded
to `org.freedesktop.Notifications`, using Plasma's existing popup and history UI.
Repeated messages replace the previous balloon for that icon. Clicking a legacy
balloon sends NIN_BALLOONUSERCLICK to its original callback window; dismissal and
timeout callbacks are also forwarded. Dead callback owners are rejected.

Modern Windows toast notifications use a separate, packaged C++/WinRT listener.
Windows requires the manifest capability and explicit notification permission:
[Microsoft notification listener documentation](https://learn.microsoft.com/en-us/windows/apps/develop/notifications/app-notifications/notification-listener).

## VM setup

1. Restart the VM and overwrite CraftRoot with the complete cumulative overlay.
2. Start Plasma using `deploy-vm.cmd`.
3. Run `install-notifications.cmd` in CraftRoot. This development build uses a
   loose app package. If Windows Developer Mode is disabled, the script opens
   its settings page; enable it in the VM and rerun the installer.
4. In the KDE Windows Notifications window, click **Enable notification access**
   and grant access in the Windows permission dialog. Declining keeps the
   listener disabled.
5. After authorization, wait for `Listener ready` in
   `%LOCALAPPDATA%\plasma-win-session\notifications.log`, then generate a new
   Windows application notification. Existing history is deliberately skipped.

The session starts the listener automatically only if the package is registered.
Use `notifications-settings.cmd` to reopen its window. The package is registered
for the current user at the existing CraftRoot location. Keep that location;
re-register after moving it. No certificate is installed and no host Windows
notification settings are changed.

## Scope and behavior

The listener polls the supported WinRT notification API every 1.5 seconds on a
worker thread. It tracks changes and removals, escapes notification text as plain
text, and keeps forwarded content in memory. The log contains status/errors,
not notification bodies. Access revocation stops polling and shows the settings
window. Only toast notifications with a ToastGeneric text binding are forwarded.

Windows can still show its original toast banner. To use KDE popups alone, turn
on **Windows Do not disturb** in the VM; keep application notifications enabled
so the listener can read them. Windows and Plasma Do Not Disturb are separate.
This build does not claim to restyle every Windows system dialog or eliminate
the original toast before Windows displays it.

Modern notifications are text mirrors. Windows does not provide the source
application's toast activation arguments through this listener API, so buttons,
inline replies and click-to-open are not fabricated. Closing a KDE mirror does
not delete the Windows original. Protected, transient or non-ToastGeneric
notifications may be unavailable. The legacy bridge only receives notifications
sent to this tray host; an Explorer-owned icon may still use Explorer's path.

To uninstall, run `install-notifications.cmd -Uninstall`. This removes this
user's package and its listener permission; legacy tray forwarding remains
available through Plasma. The isolated automated relay probe covers Unicode,
plain-text escaping, replacement, actions, close, pending cancellation and
queued updates. Registration, Windows permission UI and real application toast
delivery require VM acceptance testing.
