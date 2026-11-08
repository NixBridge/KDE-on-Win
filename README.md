# KDE on Windows

Native KDE Plasma experiments on Windows x64, using Win32/DWM with MSVC and KDE Craft. This is an experimental port, not a complete replacement for every Linux KDE service.

The Windows VM has reached the Plasma desktop with a panel, launcher, task manager, notification area and Dolphin. Runtime fixes are delivered as cumulative overlays for the original VM package. VM acceptance is still in progress.

## Components

- Qt 6.11.1, KDE Frameworks 6.28.0, Plasma 6.7.4, Dolphin 26.04.3.
- Native window integration, Windows tray bridge, Windows session actions and input/volume applets.
- KDE Breeze desktop/launcher icons, desktop KIO worker and Quick Charts notification dependencies.
- Build scripts, canonical release-source patches, runtime import validation and VM probes.

## VM use

Extract the separately built runtime to `D:\KDE\CraftRoot`. Alternatively, use `C:\KDE\CraftRoot` with drive D: free for the launcher's SUBST mapping. Apply the latest cumulative overlay to that same directory.

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

```text
build-port.ps1 / package-port.ps1   Build and full-runtime package entry points
native/                            Native CMake targets and runtime integration
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
```

Create a cumulative overlay with `python tools/make-cumulative-patch.py --version VERSION`. Generated archives, caches and local experiment output belong in ignored `dist/`, `logs/` and `work/` directories.

## AI authorship

New integration work and modifications are AI-generated/AI-assisted using OpenAI Codex, with maintainer review and VM testing. See the notice in `LICENSE` and [AI_NOTICE.md](AI_NOTICE.md). This does not attribute upstream KDE/Qt code to AI or add conditions to their licenses.
