# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the real bootstrap in private Unicode/special-character directories.

Only environment setup and menu writing run: no desktop, registry or drive mapping.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
tools = root / 'upstream/plasma-windows/tools'
with tempfile.TemporaryDirectory(prefix='relocation-', dir=root / 'work') as temporary:
    temporary = Path(temporary)
    for name in ['ordinary', '中文 space & (folder)!']:
        package = temporary / name
        (package / 'bin').mkdir(parents=True)
        (package / 'bin/plasmashell.exe').touch()  # Presence only, never executed.
        for program in ['plasma-kde-launcher.exe', 'dolphin.exe', 'plasma-open-with.exe']:
            (package / 'bin' / program).touch()
        for script in ['plasma-common.cmd', 'write-session-menu.ps1']:
            shutil.copyfile(tools / script, package / script)
        profile = temporary / '用户 & (profile)!'
        environment = os.environ.copy()
        environment.update(CRAFT_ROOT='Z:\\stale-build-root', LOCALAPPDATA=str(profile))
        output = temporary / 'environment.json'
        driver = temporary / 'check.cmd'
        driver.write_text(
            '@echo off\r\nsetlocal EnableExtensions DisableDelayedExpansion\r\n'
            'call "%RELOCATION_ROOT%\\plasma-common.cmd" :pc_setup_env\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            'call "%RELOCATION_ROOT%\\plasma-common.cmd" :pc_write_menu\r\n'
            'if errorlevel 1 exit /b 1\r\n'
            'powershell -NoProfile -Command "@{root=$env:CRAFT_ROOT;bin=$env:CRAFT_BIN;'
            'plugins=$env:QT_PLUGIN_PATH;qml=$env:QML_IMPORT_PATH} | ConvertTo-Json | '
            'Set-Content -LiteralPath $env:RELOCATION_OUTPUT -Encoding UTF8"\r\n'
            'exit /b %errorlevel%\r\n', encoding='ascii')
        environment.update(RELOCATION_ROOT=str(package), RELOCATION_OUTPUT=str(output))
        result = subprocess.run(['cmd.exe', '/d', '/v:on', '/c', str(driver)],
                                env=environment, capture_output=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        actual = json.loads(output.read_text(encoding='utf-8-sig'))
        assert Path(actual['root']) == package, actual
        assert Path(actual['bin']) == package / 'bin', actual
        assert Path(actual['plugins']) == package / 'plugins', actual
        assert Path(actual['qml']) == package / 'qml', actual
        menu = ET.parse(profile / 'menus/applications.menu').getroot()
        assert menu.findall('AppDir')[1].text == str(package / 'bin/data/applications').replace('\\', '/')
        assert len(menu.findall('Include/All')) == 1
        environment['RELOCATION_ASSOCIATIONS'] = str(root / 'runtime/install-file-associations.ps1')
        plan = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
            '[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); '
            '& $env:RELOCATION_ASSOCIATIONS -CraftRoot $env:RELOCATION_ROOT -Plan'],
            env=environment, capture_output=True, timeout=30)
        assert plan.returncode == 0, (plan.stdout, plan.stderr)
        records = json.loads(plan.stdout.decode('utf-8-sig'))
        folder_commands = [r['Value'] for r in records if r['Name'] == ''
                           and r['Path'].endswith('nixbridge-dolphin\\command')]
        assert len(folder_commands) == 3
        assert all(command.startswith('"' + str(package / 'bin/plasma-kde-launcher.exe') + '"')
                   for command in folder_commands), folder_commands

for script in ['deploy-vm.cmd', 'session-shell.cmd']:
    text = (tools / script).read_text(encoding='utf-8').lower()
    assert 'subst ' not in text and 'd:\\kde\\craftroot' not in text

manifest = json.loads((root / 'manifests/cumulative-patch-files.json').read_text())
assert manifest['repository']['write-session-menu.ps1'].endswith('write-session-menu.ps1')
for destination, source in manifest['repository'].items():
    if source.startswith('runtime/portable/'):
        assert 'D:/KDE/CraftRoot' not in (root / source).read_text(encoding='utf-8')
print('PASS: relocated bootstrap overrides stale roots; Unicode, spaces, &, (), ! and inherited delayed expansion; XML paths round-trip. No registry/desktop/drive changes.')
