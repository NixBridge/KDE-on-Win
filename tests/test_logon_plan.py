# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only credential-provider deployment plans; never register or authenticate."""
import json
from pathlib import Path
import subprocess
import os
root = Path(__file__).resolve().parents[1]
plans = []
for action in ['Install', 'Restore']:
    result = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        str(root/'runtime/install-kde-logon.ps1'), '-Action', action, '-Plan'], capture_output=True, check=True)
    plan = json.loads(result.stdout.decode('utf-8-sig'))
    assert plan['Action'] == action
    assert not plan['FiltersModified'] and not plan['SystemProvidersDisabled'] and not plan['DefaultProviderChanged']
    assert plan['Provider'].endswith('{2B2E39BE-4E8A-47AA-9FC1-E388F34F2601}')
    assert 'KDE-on-Win\\Logon\\' in plan['ProtectedTarget']
    assert not plan['ProtectedTarget'].startswith(str(root))
    plans.append(plan)
assert plans[0]['Provider'] == plans[1]['Provider']
x86=Path(os.environ['SystemRoot'])/'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
if x86.exists():
    result=subprocess.run([str(x86),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/'runtime/install-kde-logon.ps1'),'-Plan'],capture_output=True,check=True)
    assert json.loads(result.stdout.decode('utf-8-sig'))['ProtectedTarget']==plans[0]['ProtectedTarget']
print('PASS: install/restore plans use a dedicated protected DLL path and provider CLSID, without filtering or replacing system providers; no registration performed.')
