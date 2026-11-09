# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the real launcher's read-only folder plan; never launch a file manager."""
import json
import ntpath
import os
from pathlib import Path
import subprocess
import tempfile

launcher = Path(os.environ.get('KDE_CRAFT_ROOT', 'D:/KDE/CraftRoot')) / 'bin/plasma-kde-launcher.exe'


def plan(target, success=True):
    process = subprocess.run([str(launcher), '--folder-plan', str(target)], capture_output=True, timeout=15)
    assert process.returncode == (0 if success else 2), (target, process.stderr)
    return json.loads(process.stdout.decode('utf-8'))


def same(actual, expected):
    assert ntpath.normcase(ntpath.normpath(actual)) == ntpath.normcase(ntpath.normpath(str(expected))), (actual, expected)


with tempfile.TemporaryDirectory(prefix='kde-folder-routing-') as temporary:
    root = Path(temporary)
    folder = root / 'space & unicode-\u6d4b\u8bd5'
    folder.mkdir()
    named_like_shortcut = root / 'actual-directory.lnk'
    named_like_shortcut.mkdir()
    same(plan(named_like_shortcut)['directory'], named_like_shortcut)
    for target in [str(folder), str(folder) + '\\.', folder.as_uri()]:
        result = plan(target)
        same(result['directory'], folder)
        assert result['shellNamespace'] == ''
    env = os.environ.copy()
    env.update(KDE_TEST_TARGET=str(folder), KDE_TEST_LINK=str(root / 'folder.lnk'))
    script = '''$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$shell=New-Object -ComObject WScript.Shell
$link=$shell.CreateShortcut($env:KDE_TEST_LINK)
$link.TargetPath=$env:KDE_TEST_TARGET
$link.Save()
@{desktop=[Environment]::GetFolderPath('DesktopDirectory');documents=[Environment]::GetFolderPath('MyDocuments')}|ConvertTo-Json
'''
    process = subprocess.run(['powershell.exe', '-NoProfile', '-Command', script], env=env, capture_output=True, check=True)
    known = json.loads(process.stdout.decode('utf-8-sig'))
    same(plan(root / 'folder.lnk')['directory'], folder)
    if known['desktop'] and Path(known['desktop']).is_dir():
        for target in ['shell:Desktop', 'desktop:/', '::{00021400-0000-0000-C000-000000000046}', 'shell:Desktop\\.']:
            same(plan(target)['directory'], known['desktop'])
    if known['documents'] and Path(known['documents']).is_dir():
        same(plan('shell:Personal')['directory'], known['documents'])
    same(plan(Path(folder.anchor))['directory'], folder.anchor)
    document = root / 'document.txt'
    document.write_text('Do not execute or open this document', encoding='utf-8')
    for target in [document, root / 'missing', 'shell:NotARealKnownFolder']:
        assert plan(target, success=False) == {'directory': '', 'shellNamespace': ''}
    virtual = plan('shell:ControlPanelFolder')
    assert virtual['directory'] == '' and virtual['shellNamespace'] == 'shell:ControlPanelFolder'

print('PASS: actual folder launcher plans for Unicode paths, file URLs, drive roots, directory shortcuts, Desktop/Documents aliases and virtual namespace fallback; no applications launched or associations changed.')
