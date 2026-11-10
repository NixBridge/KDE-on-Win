# SPDX-License-Identifier: GPL-3.0-or-later
"""Real PowerShell 5.1 file replacement and harmless desktop-process surrogates."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
tools=root/'upstream/plasma-windows/tools'
script=(root/'runtime/install-file-associations.ps1').read_text()
replace=re.search(r"\[IO.File\]::Replace\(\$temporary,\$backupPath,[^)]*\)",script).group()
with tempfile.TemporaryDirectory(prefix='startup-hotfix-',dir=root/'work') as folder:
    base=Path(folder)
    fixture=base/'replace.ps1'
    fixture.write_text("$ErrorActionPreference='Stop'; $temporary=Join-Path $env:FIXTURE_ROOT 'new'; $backupPath=Join-Path $env:FIXTURE_ROOT 'backup'; "
        + "[IO.File]::WriteAllText($backupPath,'original'); foreach($value in @('second','third')){[IO.File]::WriteAllText($temporary,$value); "
        + replace + "; if([IO.File]::ReadAllText($backupPath) -cne $value){throw 'replacement failed'}}; "
        + "if([IO.File]::Exists($temporary)){throw 'temporary not consumed'}",encoding='utf-8')
    env=os.environ.copy();env['FIXTURE_ROOT']=str(base)
    result=subprocess.run(['powershell.exe','-NoProfile','-File',str(fixture)],env=env,capture_output=True,timeout=15)
    assert result.returncode==0,(result.stdout,result.stderr)
    package=base/'runtime space & (test)!';(package/'bin').mkdir(parents=True)
    shutil.copyfile(tools/'plasma-common.cmd',package/'plasma-common.cmd')
    driver=base/'check.cmd'
    driver.write_text('@echo off\nsetlocal DisableDelayedExpansion\ncall "%FIXTURE_COMMON%" :pc_run_plasma\nexit /b %errorlevel%\n',encoding='ascii')
    env.update(CRAFT_BIN=str(package/'bin'),FIXTURE_COMMON=str(package/'plasma-common.cmd'),LOCALAPPDATA=str(base/'profile'))
    env.pop('KDE_ON_WIN_SETTINGS',None)
    system=Path(os.environ['SystemRoot'])/'System32'
    for program,expected in [('whoami.exe',0),('where.exe',2)]:
        shutil.copyfile(system/program,package/'bin/plasmashell.exe')
        result=subprocess.run(['cmd.exe','/d','/v:on','/c',str(driver)],env=env,capture_output=True,timeout=15)
        assert result.returncode==expected,(result.returncode,result.stdout,result.stderr)
        log=base/'profile/plasma-win-session/logs/plasmashell.exe.log'
        assert log.is_file() and log.stat().st_size>0
        if expected: assert b'ERROR: plasmashell exited with code 2.' in result.stdout
    # Logging must not prevent launch if the log directory is unavailable.
    shutil.copyfile(system/'whoami.exe',package/'bin/plasmashell.exe')
    blocked=base/'not-a-directory';blocked.write_text('fixture')
    env['KDE_ON_WIN_SETTINGS']=str(blocked/'appearance.ini')
    result=subprocess.run(['cmd.exe','/d','/c',str(driver)],env=env,capture_output=True,timeout=15)
    assert result.returncode==0 and b'Cannot write Plasma log' in result.stdout,(result.stdout,result.stderr)
assert 'call "%~dp0plasma-common.cmd" :pc_run_plasma' in (tools/'deploy-vm.cmd').read_text()
print('PASS: real PowerShell repeated atomic backup replacement; foreground stdout/stderr capture, failure exit code, special paths and unwritable-log fallback. No Plasma or registry changes.')
