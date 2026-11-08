# Windows built-in program icons

`windowsiconmap.cpp` maps known executable names, management-console files and Store AppUserModelIDs to functionally equivalent Breeze icons. It checks that a candidate exists with KIconLoader before selecting it. Shortcut targets and arguments are read once through the shared `windowsshortcut` helper; `.msc` targets are resolved from console arguments. Application names remain shortcut filenames rather than descriptions.

Unknown programs receive the appropriate generic Breeze file/application icon. This does not claim that KDE's replacement icon is the original Windows brand artwork. Third-party program names are not matched by loose executable substrings.

The following representative mappings were verified against actual embedded Breeze resource paths:

| Windows program / App ID | Selected Breeze icon |
| --- | --- |
| `notepad.exe` | `accessories-text-editor` |
| `calc.exe` | `accessories-calculator` |
| `explorer.exe` | `system-file-manager` |
| `mspaint.exe` | `kolourpaint` |
| `cmd.exe` | `utilities-terminal` |
| `powershell.exe` | `utilities-terminal` |
| `taskmgr.exe` | `utilities-system-monitor` |
| `regedit.exe` | `preferences-system` |
| `control.exe` | `preferences-system` |
| `snippingtool.exe` | `spectacle` |
| `mstsc.exe` | `krdc` |
| `magnify.exe` | `preferences-desktop-accessibility` |
| `osk.exe` | `input-keyboard` |
| `charmap.exe` | `accessories-character-map` |
| `wmplayer.exe` | `multimedia-player` |
| `msedge.exe` | `internet-web-browser` |
| `eventvwr.msc` | `utilities-log-viewer` |
| `services.msc` | `preferences-system-services` |
| `devmgmt.msc` | `preferences-devices` |
| `apps:Microsoft.WindowsCalculator_8wekyb3d8bbwe!App` | `accessories-calculator` |
| `apps:Microsoft.Windows.Photos_8wekyb3d8bbwe!App` | `org.kde.gwenview` |
| `apps:Microsoft.WindowsStore_8wekyb3d8bbwe!App` | `plasmadiscover` |

The catalog contains additional candidates for Terminal, device management, accessibility, security, notes, camera and recording tools. Missing candidates fall through safely rather than displaying an empty icon. Run `breeze-mapping-probe.exe -platform offscreen` in a built runtime to check the representative catalog.
