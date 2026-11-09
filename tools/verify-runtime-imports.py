"""Check x64 PE imports of the runtime entry points without starting the shell."""
import argparse
import json
import os
from pe_imports import imports
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
args = parser.parse_args()
runtime = args.root.resolve()
binary_dir = runtime / 'bin'
system_dir = Path(os.environ['SystemRoot']) / 'System32'
files = {p.name.lower(): p for p in binary_dir.glob('*.dll')}


queue = [binary_dir / name for name in ['plasmashell.exe', 'krunner.exe', 'kded6.exe', 'kactivitymanagerd.exe', 'dolphin.exe', 'konsole.exe', 'plasma-process-monitor.exe', 'plasma-kde-launcher.exe', 'dbus-daemon.exe', 'trayhost.exe', 'shellswitch.exe', 'kwindowsystem-probe.exe']]
queue += [runtime / relative for relative in ['plugins/platforms/qwindows.dll', 'plugins/sqldrivers/qsqlite.dll', 'qml/org/kde/plasma/windowsmenu/startmenuplugin.dll']]
queue += [binary_dir / name for name in ['plasma-text-editor.exe', 'plasma-system-settings.exe', 'plasma-window-decorations.exe', 'plasma-notification-bridge.exe', 'notification-relay-probe.exe']]
queue += [binary_dir / name for name in ['plasma-archive-manager.exe', 'konsole-session-probe.exe', 'plasma-personalization.exe', 'personalization-probe.exe', 'startmenu-model-probe.exe']]
for directory in ['qml', 'plugins']:
    queue.extend((runtime / directory).rglob('*.dll'))
seen = set()
missing = []
while queue:
    path = queue.pop()
    key = str(path).lower()
    if key in seen:
        continue
    seen.add(key)
    if not path.exists():
        missing.append({'module': str(path), 'missing': '<entry point>'})
        continue
    for name in imports(path):
        key = name.lower()
        if key.startswith(('api-ms-', 'ext-ms-')):
            continue
        dependency = files.get(key)
        if dependency:
            queue.append(dependency)
        elif (path.parent / name).exists():
            queue.append(path.parent / name)
        elif (system_dir / name).exists():
            continue
        else:
            missing.append({'module': str(path), 'missing': name})
report = {'modules_checked': len(seen), 'architecture': 'x64', 'missing': missing}
(Path(__file__).resolve().parents[1] / 'manifests' / 'runtime-import-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
raise SystemExit(1 if missing else 0)
