# SPDX-License-Identifier: GPL-3.0-or-later
"""Run only the host's isolated self-test while its log directory is blocked."""
import os
import subprocess
import tempfile
from pathlib import Path

binary = Path(os.environ.get('KDE_CRAFT_ROOT', 'D:/KDE/CraftRoot')) / 'bin/kde-session-host.exe'
with tempfile.TemporaryDirectory(prefix='kde-log-fallback-') as folder:
    root = Path(folder)
    blocked = root / 'logs'
    blocked.write_bytes(b'preserve this ordinary file')
    env = os.environ.copy()
    env['KDE_ON_WIN_SETTINGS'] = str(root / 'appearance.ini')
    result = subprocess.run([str(binary), '--self-test'], env=env, capture_output=True, timeout=15)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert blocked.read_bytes() == b'preserve this ordinary file'
print('PASS: hidden synthetic children start with NUL redirection when logs cannot be created; blocker preserved.')
