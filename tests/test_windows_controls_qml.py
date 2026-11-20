# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the desktop components that use the native Windows Controls style."""
import os
import subprocess
from pathlib import Path

runtime = Path(os.environ['KDE_CRAFT_ROOT']).resolve()
env = os.environ.copy()
env.update(
    QT_QPA_PLATFORM='offscreen',
    QT_QUICK_BACKEND='software',
    QT_QUICK_CONTROLS_STYLE='Windows',
    QT_PLUGIN_PATH=str(runtime / 'plugins'),
    QML_IMPORT_PATH=str(runtime / 'qml'),
    QML2_IMPORT_PATH=str(runtime / 'qml'),
)
plasma = runtime / 'bin/data/plasma'
for relative in (
    'plasmoids/org.kde.desktopcontainment/contents/ui/RenameEditor.qml',
    'plasmoids/org.kde.desktopcontainment/contents/ui/FolderViewLayer.qml',
    'plasmoids/org.kde.plasma.windowsmenu/contents/ui/main.qml',
    'shells/org.kde.plasma.desktop/contents/applet/AppletError.qml',
):
    result = subprocess.run(
        [runtime / 'bin/qml-import-probe.exe', '--check-component', plasma / relative],
        env=env, capture_output=True, timeout=10,
    )
    if result.returncode:
        raise SystemExit(f'{relative}: {result.stdout.decode("utf-8", errors="replace")}\n'
                         f'{result.stderr.decode("utf-8", errors="replace")}')
    print('PASS (Windows Controls): ' + relative)
