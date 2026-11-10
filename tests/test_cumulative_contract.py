# SPDX-License-Identifier: GPL-3.0-or-later
"""The overlay must actually ship the current session/shell recovery entrypoints."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'manifests/cumulative-patch-files.json').read_text(encoding='utf-8'))
for name in ['deploy-vm.cmd', 'session-shell.cmd', 'plasma-common.cmd']:
    source = manifest['repository'].get(name)
    assert source == 'upstream/plasma-windows/tools/' + name, (name, source)
    data = (root / source).read_bytes()
    assert not data.startswith(b'\xef\xbb\xbf')
    assert b'\r\n' in data and b'\n' not in data.replace(b'\r\n', b'')
    assert not (root / 'runtime' / name).exists(), 'duplicate runtime entrypoint'
text = (root / manifest['repository']['deploy-vm.cmd']).read_text(encoding='utf-8')
assert ':restore_shell' in text and 'shell-registry.ps1' in text
transaction=(root/'upstream/plasma-windows/tools/shell-registry.ps1').read_text(encoding='utf-8')
assert 'kde-session-host.exe' in transaction and '[IO.File]::Exists($native)' in transaction
assert 'session-shell.cmd' in transaction and 'CreateNew' in transaction
print('PASS: cumulative overlay includes canonical startup, shell-install and Explorer recovery scripts in runtime encoding.')

assert manifest['repository']['plasma-shell.cmd'] == 'upstream/plasma-windows/tools/plasma-shell.cmd'
assert 'bin/plasma-desktop-tools.exe' in manifest['runtime']
common=(root/manifest['repository']['plasma-common.cmd']).read_text(encoding='utf-8')
bus=common[common.index(':pc_start_bus\n'):common.index('rem :pc_start_services')]
assert 'exit /b %errorlevel%' in bus
for name in ['deploy-vm.cmd','session-shell.cmd','plasma-shell.cmd']:
    content=(root/manifest['repository'][name]).read_text(encoding='utf-8')
    assert ':pc_start_bus\nif errorlevel 1 exit /b 1' in content, name
print('PASS: new helper and secondary shell entrypoint ship; all VM entrypoints propagate bus startup failure.')

assert manifest['repository']['shell-registry.ps1'] == 'upstream/plasma-windows/tools/shell-registry.ps1'
for name in ['deploy-vm.cmd','plasma-shell.cmd']:
    text=(root/manifest['repository'][name]).read_text(encoding='utf-8')
    assert text.count('shell-registry.ps1" -Action')==2
assert 'bin/data/applications/org.nixbridge.quickactions.desktop' in manifest['runtime']
print('PASS: canonical idempotent Shell transaction and Quick Actions entry ship in cumulative overlay.')

# These native applet DLLs contain controllers or plugin metadata; QML overlays alone do not update them.
# Their existence in the original archive must not hide later lifecycle fixes.
for name in ['org.kde.plasma.imewin.dll','org.kde.plasma.volumewin.dll','org.kde.plasma.windowsmenu.dll']:
    assert 'plugins/plasma/applets/' + name in manifest['runtime'], name
print('PASS: native IME, volume and launcher applet binaries are shipped by the cumulative overlay.')

for name in ['trayhost.exe','shellswitch.exe','titlebar.exe','kwindowsystem-probe.exe','desktop-launch-probe.exe']:
    assert 'bin/' + name in manifest['runtime'], name
print('PASS: updated tray host, shell switcher and remaining native build products are cumulative, not inherited from the base archive.')
