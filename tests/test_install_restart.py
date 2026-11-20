# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise install-only and the restart decision with a simulated registry and shutdown."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
tools = root / 'upstream/plasma-windows/tools'
with tempfile.TemporaryDirectory(prefix='install-test-', dir=root / 'work') as temporary:
    base = Path(temporary)
    package = base / '中文 & (install)!'
    (package / 'bin').mkdir(parents=True)
    (package / 'bin/plasmashell.exe').touch()
    for name in ('deploy-vm.cmd', 'plasma-common.cmd'):
        shutil.copyfile(tools / name, package / name)
    (package / 'shell-registry.ps1').write_text(
        "param([string]$Action)\n"
        "if ($Action -ne 'install') { exit 7 }\n"
        "exit ([int]$env:TEST_INSTALL_EXIT)\n", encoding='ascii')
    installer = (root / 'runtime/install.cmd').read_text(encoding='ascii')
    installer = installer.replace('"%SystemRoot%\\System32\\choice.exe"', 'call "%TEST_CHOICE%"')
    installer = installer.replace('"%SystemRoot%\\System32\\shutdown.exe"', 'call "%TEST_SHUTDOWN%"')
    installer = installer.replace('    pause\n', '')
    (package / 'install.cmd').write_text(installer, encoding='ascii')
    choice = base / 'choice.cmd'
    choice.write_text('@echo off\necho choice called\nexit /b %TEST_CHOICE_EXIT%\n', encoding='ascii')
    shutdown = base / 'shutdown.cmd'
    shutdown.write_text('@echo off\necho shutdown %*\nexit /b 0\n', encoding='ascii')
    driver = base / 'check.cmd'
    driver.write_text('@echo off\nsetlocal DisableDelayedExpansion\ncall "%TEST_INSTALLER%"\nexit /b %errorlevel%\n',
                      encoding='ascii')
    env = os.environ | {'TEST_CHOICE': str(choice), 'TEST_SHUTDOWN': str(shutdown),
                        'TEST_INSTALLER': str(package / 'install.cmd'), 'USERPROFILE': str(base)}
    for install_code, choice_code, expected_exit, expect_restart in (
        ('7', '1', 1, False), ('0', '2', 0, False), ('0', '1', 0, True),
    ):
        result = subprocess.run(
            ['cmd.exe', '/d', '/c', str(driver)], capture_output=True, timeout=20,
            env=env | {'TEST_INSTALL_EXIT': install_code, 'TEST_CHOICE_EXIT': choice_code},
        )
        assert result.returncode == expected_exit, result
        assert (b'choice called' in result.stdout) == (install_code == '0'), result
        assert (b'shutdown /r /t 0' in result.stdout) == expect_restart, result
        assert b'1/5 Environment' not in result.stdout, result
print('PASS: install-only, restart on success, postpone and failure handling. No real registry writes or restart.')
