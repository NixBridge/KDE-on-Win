# KDE-style desktop tools on Windows

The Plasma menu and initial panel provide Konsole, Dolphin and Process Monitor.
Existing panels gain the terminal and monitor once; later user unpinning is preserved.
You can also run `start-konsole.cmd`, `start-dolphin.cmd` or
`start-process-monitor.cmd` from CraftRoot after applying the cumulative overlay.

Konsole 26.04.3 is the real KDE terminal, built with its native Windows ConPTY
backend. It runs Windows shells such as PowerShell and CMD; a Linux shell is not
bundled. Standard shell shortcuts without custom arguments in the Plasma menu
are routed through Konsole. Shortcuts with custom arguments retain their original
launch behavior.

Process Monitor is this project's Qt Widgets application, not a port of official
Plasma System Monitor. It displays real Windows processes, PID, CPU usage and
working-set memory, with filtering, sorting and a two-second refresh. Ending a
process requires confirmation and rejects itself, critical processes and stale
process identities. It does not elevate privileges or replace Ctrl+Shift+Esc.

The scoped launcher applies the Breeze widget style and custom titlebar to the
main windows of these three applications. Menus remain available; the titlebar
supports minimize, maximize, restore and close, with native resize hit testing.
Windows still uses DWM. This does not theme arbitrary third-party windows.

Headless checks cover actual ConPTY arithmetic output and resize, process
enumeration and termination of a test-owned child, filter behavior, icon mapping,
and titlebar controls. Actual Konsole and Dolphin load the style successfully.
Interactive dragging, resizing, snapping, Chinese input and multi-monitor behavior
still require acceptance testing inside the Windows VM.
