# SPDX-License-Identifier: GPL-3.0-or-later
"""Package every runtime fix needed on top of the original VM archive."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--runtime', type=Path, default=Path('D:/KDE/CraftRoot'))
parser.add_argument('--version', required=True)
args = parser.parse_args()
if not args.version.replace('-', '').isalnum():
    parser.error('version must contain only letters, digits and hyphens')
root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'manifests' / 'cumulative-patch-files.json').read_text())
out = root / 'dist' / args.version
stage = out / 'patch'
sources = {relative: args.runtime / relative for relative in manifest['runtime']}
sources.update({relative: root / source for relative, source in manifest['repository'].items()})
missing = [str(source) for source in sources.values() if not source.is_file()]
if missing:
    raise SystemExit('Missing cumulative patch files: ' + ', '.join(missing))
for relative, source in sources.items():
    target = stage / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
(stage / 'README-CUMULATIVE.txt').write_text(
    'Cumulative overlay for the ORIGINAL Plasma Windows VM archive. Includes all previous fixes.\n'
    'Restart the VM, extract into the existing CraftRoot and overwrite files. Start deploy-vm.cmd.\n'
    'Desktop and launcher use KDE Breeze icons. Windows shortcuts open through the Shell, preserving their arguments.\n'
    'Only the Plasma panel is kept above Explorer taskbars; the desktop wallpaper is not made topmost.\n'
    'Run apply-panel-opacity.cmd after startup if the existing panel is transparent.\n'
    'Built-in Windows programs use equivalent Breeze icons; QWidget context menus use an opaque topmost Windows path.\n'
    'Unavailable Linux default taskbar pins are repaired automatically after startup. Run repair-taskbar-pins.cmd to retry in a running session.\n'
    'Includes real Konsole (Windows shells), a project-owned process monitor and scoped Breeze titlebars for Konsole/Dolphin/monitor.\n'
    'Launch through the Plasma menu or start-konsole.cmd, start-dolphin.cmd and start-process-monitor.cmd.\n'
    'Includes Breeze standard Windows captions, project-owned System Settings and Text Editor. Custom titlebars are skipped.\n'
    'For modern Windows notification forwarding, run install-notifications.cmd in the VM and grant notification access.\n'
    'Legacy tray balloons are forwarded by trayhost. Native Windows toast banners can still appear; Windows Do Not Disturb suppresses those.\n'
    'System Settings now includes Panel, Desktop, Start Menu and Diagnostics pages. See docs/PERSONALIZATION.md.\n'
    'Fix12 includes native system controls, ConPTY close fixes, safe native taskbar behavior and KDE Archive Manager.\n'
    'The VM session registers Dolphin folder defaults and archive fallbacks once. Existing protected ZIP defaults need Open With > Always.\n'
    'See docs/FIX12.md for supported controls, association restore, archive limits and VM acceptance steps.\n'
    'Fix13 adds Chinese project UI, full-caption/restore fixes and synchronized KDE colors, accent and fonts. See docs/FIX13.md.\n'
    'Fix14 constrains wallpaper Z order before native raises and disables DWM transitions only on decorated windows to avoid native-caption flashes. See docs/FIX14.md.\n'
    'Fix15 recolors Breeze caption glyphs against each actual titlebar background, including inactive and disabled modes. See docs/FIX15.md.\n'
    'Fix16 routes all filesystem folder classes and open verbs to Dolphin, including desktop directory shortcuts and physical Shell folders. Windows-only virtual pages retain their native handlers. See docs/FIX16.md.\n'
    'Fix17 adds event-driven caption attachment and the KDE application chooser. Configure MAILTO through configure-kde-link-chooser.cmd if Windows has an existing protected default. Password logon replacement remains unimplemented. See docs/FIX17.md and docs/STYLE_SCOPE.md.\n'
    'Fix18 adds opt-in classic-control painting (apply-client-style.cmd), KDE Qt dialogs and an experimental local-account login tile (install-kde-logon.cmd). Full LogonUI replacement is not implemented. See docs/FIX18.md and docs/LOCAL_LOGON.md before VM testing.\n'
    'Fix19 hardens login DLL publication, style restoration, service arguments and normal-session recovery. See docs/FIX19.md.\n'
    'Fix20 adds editor find/replace, line navigation, wrap/zoom and process-monitor pause/manual refresh. See docs/FIX20.md.\n'
    'Fix21 repairs startup overlay omissions, session handle inheritance, tray teardown/protocol bounds, notification callback lifetime and COM cleanup. See docs/FIX21.md.\n'
    'Fix22 adds desktop menu tools, taskbar command/grouping fixes, fullscreen panel priority, safe shortcut alternatives and in-memory clipboard history. Session restore is not implemented or enabled. See docs/FIX22.md.\n'
    'Fix23 improves desktop-tools focus, empty-history feedback, hidden refresh and settings-error recovery. See docs/FIX23.md.\n'
    'Full VM interaction and stacking acceptance still require testing.\n', encoding='utf-8')
archive = out / ('plasma-windows-' + args.version + '.zip')
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for file in sorted(stage.rglob('*')):
        if file.is_file():
            z.write(file, file.relative_to(stage).as_posix())
with zipfile.ZipFile(archive) as z:
    assert set(sources) <= set(z.namelist())
    assert z.testzip() is None
with archive.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
Path(str(archive) + '.sha256').write_text(digest + ' *' + archive.name + '\n', encoding='utf-8')
print(json.dumps({'archive': str(archive), 'bytes': archive.stat().st_size,
                  'sha256': digest, 'cumulative_runtime_files': len(sources)}, indent=2))
