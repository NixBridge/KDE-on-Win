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
assert 'kde-session-host.exe' in text and ':restore_shell' in text
print('PASS: cumulative overlay includes canonical startup, shell-install and Explorer recovery scripts in runtime encoding.')
