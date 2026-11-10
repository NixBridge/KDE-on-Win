# KDE on Windows

Native KDE Plasma experiments on Windows x64, using Win32/DWM with MSVC and KDE Craft. This is an experimental port, not a complete replacement for every Linux KDE service.

The Windows VM has reached the Plasma desktop with a panel, launcher, task manager, notification area and Dolphin. Runtime fixes are delivered as cumulative overlays for the original VM package. VM acceptance is still in progress.

## Components

- Qt 6.11.1, KDE Frameworks 6.28.0, Plasma 6.7.4, Dolphin 26.04.3.
- Native window integration, Windows tray bridge, Windows session actions and input/volume applets.
- KDE Breeze desktop/launcher icons, desktop KIO worker and Quick Charts notification dependencies.
- Build scripts, canonical release-source patches, runtime import validation and VM probes.

## VM use

Extract the separately built runtime to a directory of your choice. Apply the latest cumulative overlay to that same directory; Fix24 and later do not require drive D: or SUBST.

Run `deploy-vm.cmd` without arguments for a temporary session. Run `run-runtime-checks.cmd` to check QML imports and `desktop:/`. The experimental user-shell install/restore options are documented in `docs/PORTING.md`. Runtime logs include `%TEMP%\plasmashell-debug.log`.

The repository does not contain the multi-gigabyte runtime ZIP, build cache or personal VM files.

## Build

Use Windows x64, Visual Studio 2022 Build Tools, CMake/Ninja and an NTFS Craft root. Initialize KDE Craft and blueprints, checking out the exact commits in `manifests/build-lock.json`. The current scripts expect `D:\KDE\CraftRoot` and `D:\KDE\blueprints`; `interfaces/CMakeLists.txt` also requires the official interface source trees listed in `manifests/extra-source-verification.json` under `D:\KDE\sources\extra`.

Run `build-port.ps1`. It applies the blueprint overlay, builds the patched components and native modules, then stages and packages the runtime. See `docs/PORTING.md` for the actual build status and limitations. The checkout used for the original build has additional bootstrap/download helpers; a fresh Craft setup remains a prerequisite.

## Source and licensing

This project builds on [ChPu437/plasma-windows](https://github.com/ChPu437/plasma-windows), pinned at `7078fc706f780c4c28e90f222467a04d816df48b`, with substantial integration fixes. Its modified source is vendored under `upstream/plasma-windows` to keep the port reviewable without a mutable submodule reference.

New, independently authored integration code is licensed under **GPL-3.0-or-later**; see `LICENSE`. Vendored code and patches retain their applicable upstream licenses and copyright notices. This root license does not relicense third-party code. See `THIRD_PARTY.md`.

Source archive hashes and applied patch counts are recorded in `manifests/source-verification.json`. No upstream dependency binaries are committed here. Windows built-in icon substitutions are documented in [docs/WINDOWS_ICONS.md](docs/WINDOWS_ICONS.md).

## Repository layout

The native terminal, process monitor and scoped Breeze window appearance are
documented in [docs/DESKTOP_TOOLS.md](docs/DESKTOP_TOOLS.md).
Global standard-caption decorations and settings/editor replacements are covered
in [docs/WINDOW_DECORATIONS.md](docs/WINDOW_DECORATIONS.md). Windows notification
bridging and its VM permission setup are in [docs/NOTIFICATIONS.md](docs/NOTIFICATIONS.md).
Start menu folders, icons, search and live panel/wallpaper preferences are covered
in [docs/PERSONALIZATION.md](docs/PERSONALIZATION.md).

```text
build-port.ps1 / package-port.ps1   Build and full-runtime package entry points
native/                            Native CMake targets and runtime integration
  apps/                            Scoped launcher, process monitor and desktop entries
  style/                           First-party Breeze main-window chrome
  cmake/                           Plasma module and probe target definitions
  probes/                          Headless/VM acceptance executables
tools/                             Blueprint, verification and packaging tools
  build/                           Local MSVC build wrappers with shared setup
  maintenance/                     Historical bootstrap and patch repair tools
runtime/                           Scripts copied into VM packages
manifests/                         Source pins, verification and cumulative file list
interfaces/                        Official D-Bus interface generation
docs/                              Port status and implementation notes
upstream/plasma-windows/            Vendored source and canonical source patches
third_party/                       Redistributed upstream license notices
```

Create a cumulative overlay with `python tools/make-cumulative-patch.py --version VERSION`. Generated archives, caches and local experiment output belong in ignored `dist/`, `logs/` and `work/` directories.

## AI authorship

New integration work and modifications are AI-generated/AI-assisted using OpenAI Codex, with maintainer review and VM testing. See the notice in `LICENSE` and [AI_NOTICE.md](AI_NOTICE.md). This does not attribute upstream KDE/Qt code to AI or add conditions to their licenses.

Current integration fixes and direct system controls are documented in [docs/FIX12.md](docs/FIX12.md), including cumulative overlay use and reversible file associations.

Latest startup repair: [Fix25 Hotfix4 code review](docs/FIX25-CODE-REVIEW.md) fixes reproducible panel startup null dereferences and restores omitted native applet/tray/shell-switch binaries to the cumulative overlay.

Latest cumulative integration: [Fix25](docs/FIX25.md) hardens Shell backups/recovery and native window-event lifetime, adds Places/Quick Actions, opt-in Windows theme following, shortcut failure details and previewable private diagnostics. Includes [Fix24](docs/FIX24.md), which removes the fixed drive/path requirement; extract the complete runtime to any local folder and run `deploy-vm.cmd`. Includes [Fix23](docs/FIX23.md), which fixes desktop-tools focus, hidden-list refresh and stale settings errors, with clearer empty-history feedback. Includes [Fix22](docs/FIX22.md), which improves taskbar commands/grouping, fullscreen panel priority, input labels, notification restart safety and bounded startup recovery. Adds desktop menu actions, configurable safe shortcut alternatives and memory-only clipboard history. Includes [Fix21](docs/FIX21.md) and all earlier updates. Optional application session restore remains unimplemented and disabled. Full LogonUI, WinUI/XAML and arbitrary client-area replacement remain outside the implemented coverage; see [style coverage](docs/STYLE_SCOPE.md).

Fix25 startup hotfix: fixes Windows PowerShell 5.1 association-backup replacement and captures foreground Plasma startup errors. See [startup hotfix notes](docs/FIX25-STARTUP-HOTFIX.md).

Fix25 startup hotfix2 captures the entire normal bootstrap in `startup-diagnostic.log` and keeps failures visible. VM startup failure is still under investigation; titlebar styling alone does not indicate a working Plasma session.

Fix25 startup hotfix3 confirms the reported Plasma exit is access violation 0xC0000005. Qt errors now also reach stderr/startup logs; the VM-specific crash cause remains unresolved. See the startup hotfix notes.
