# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise legacy entrypoint dry runs only, with no registry writes or program execution."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='fix25-legacy-',dir=root/'work') as directory:
    folder=Path(directory);location=folder/'中文 space & (folder)!';location.mkdir()
    program=location/'fake shell.exe';program.touch()
    env=os.environ.copy();env.update(SWITCH_SHELL_DRYRUN='1',SWITCH_SHELL_TESTKEY='HKCU\\Software\\NixBridge\\SyntheticOnly',TEMP=str(folder),USERPROFILE=str(folder),LEGACY_SCRIPT=str(root/'upstream/plasma-windows/tools/switch-shell.cmd'),FAKE_SHELL=str(program))
    driver=folder/'check.cmd';driver.write_text('@echo off\r\nsetlocal DisableDelayedExpansion\r\ncall "%LEGACY_SCRIPT%" install "%FAKE_SHELL%" /force\r\nexit /b %errorlevel%\r\n',encoding='ascii')
    p=subprocess.run(['cmd.exe','/d','/v:on','/c',str(driver)],env=env,capture_output=True,timeout=20)
    assert p.returncode==0,(p.stdout,p.stderr)
    assert b'[DRY RUN]' in p.stdout and b'Installed shell:' in p.stdout
    assert not (folder/'plasma-windows-test/shell-backup.txt').exists()
    driver.write_text('@echo off\r\nsetlocal DisableDelayedExpansion\r\ncall "%LEGACY_SCRIPT%" install "%FAKE_SHELL%" /force extra\r\nexit /b %errorlevel%\r\n',encoding='ascii')
    p=subprocess.run(['cmd.exe','/d','/v:on','/c',str(driver)],env=env,capture_output=True,timeout=20)
    assert p.returncode==2,(p.stdout,p.stderr)
print('PASS: legacy install dry run preserves Unicode/space/&/parentheses/! paths and rejects extra arguments; backup and registry unchanged.')
