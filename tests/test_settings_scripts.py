# SPDX-License-Identifier: GPL-3.0-or-later
"""Parse the update scripts with Windows PowerShell without executing them."""
import re
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
scripts=re.findall(r'R"PS\((.*?)\)PS"',(root/'native/apps/system-updates.cpp').read_text(encoding='utf-8'),re.S)
assert len(scripts)==2
with tempfile.TemporaryDirectory(prefix='settings-parser-',dir=root/'work') as folder:
    folder=Path(folder).resolve();assert folder.is_relative_to(root.resolve())
    driver=folder/'parse.ps1'
    driver.write_text("param([string]$Source)\n$tokens=$null;$errors=$null;[System.Management.Automation.Language.Parser]::ParseFile($Source,[ref]$tokens,[ref]$errors)|Out-Null;if($errors.Count){$errors|ForEach-Object{Write-Error $_};exit 1};exit 0\n",encoding='utf-8')
    for index,script in enumerate(scripts):
        script=script.replace('%1',"'00000000-0000-0000-0000-000000000001:1'='"+'a'*64+"'")
        source=folder/f'update-{index}.ps1';source.write_text(script,encoding='utf-8')
        subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(driver),'-Source',str(source)],capture_output=True,check=True)
    batch=(root/'upstream/plasma-windows/tools/deploy-vm.cmd').read_text(encoding='utf-8')
    session_commands=re.findall(r'^powershell -NoProfile -Command "(.*)"$',batch,re.M)
    assert len(session_commands)==3
    for index,command in enumerate(session_commands):
        source=folder/f'session-{index}.ps1';source.write_text(command,encoding='utf-8')
        subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(driver),'-Source',str(source)],capture_output=True,check=True)
    for source in [root/'runtime/start-window-decorations-admin.ps1', root/'runtime/install-file-associations.ps1', root/'runtime/install-kde-logon.ps1', root/'runtime/logon-deployment.ps1']:
        subprocess.run(['powershell.exe','-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(driver),'-Source',str(source)],capture_output=True,check=True)
print('PASS: update, elevated-decoration and association script syntax; no APIs or installation scripts executed.')
