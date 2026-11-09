# Focused checks

* `python tests/test_msvc_deps.py`: UTF-8/CP936 Chinese and English compiler
  dependency lines; diagnostics pass through unchanged.
* `node tests/taskbar-migration.cjs`: preserve custom launchers and later unpins.
* Built `startmenu-model-probe.exe -platform offscreen`: merged folders, duplicate
  shortcuts, ordinary files, search/back navigation, native icon retention and
  AppsFolder cache. It reads installed app metadata but does not launch apps.
* `qml-import-probe.exe -platform offscreen --check-startmenu-model`: load the
  actual installed plugin and verify properties/search, catching stale object ABI.
* `personalization-probe.exe`: execute the real generated scripts in a JS engine,
  including option bounds and quoted/Unicode wallpaper paths. No live desktop is
  modified by this probe.

For compiler integration, use an x64 VS Developer Prompt and configure
`tests/fixtures/msvc-deps` with Ninja, `-DCMAKE_CXX_COMPILER=cl` and
`-DCMAKE_PROJECT_INCLUDE=<absolute repo>/tools/build/msvc-deps.cmake`.
Build it, inspect `ninja -C <build> -t deps CMakeFiles/probe.dir/main.cpp.obj`,
then modify only `依赖.hpp`. `ninja -C <build> -n` must schedule the object again.
The fixture creates no visible windows or OS settings.


Fix21 isolated regression suite: `python tests/run_regressions.py --runtime D:/KDE/CraftRoot`.
This runs 49 checks, using private settings, offscreen Qt rendering and test-owned hidden HWNDs.
It never starts a real desktop session, installs global hooks or changes login/association registration.
The tray and callback probes avoid the live system tray and notification permissions; notification relay uses a private temporary D-Bus daemon.


Fix22 expands the same runner to 57 checks and writes `work/fix22-test-results.json`.
The clipboard/history tests use synthetic MIME data and a private per-process IPC endpoint; they never access the system clipboard or register real hotkeys.
The new window-actions probe uses only child HWNDs under a hidden test parent. Fullscreen geometry checks do not constitute multi-monitor or game acceptance.
Chinese offscreen UI checks explicitly load Windows fonts in the test process and assert actual glyph coverage.
