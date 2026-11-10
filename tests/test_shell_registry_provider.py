# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify the actual registry provider against a unique scratch key, never Winlogon."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='fix25-registry-',dir=root/'work') as directory:
    folder=Path(directory).resolve();assert folder.is_relative_to((root/'work').resolve())
    driver=folder/'verify.ps1'
    driver.write_text(r'''$ErrorActionPreference='Stop'
$prefix='Software\NixBridge\KDE-on-Win\TestKeys\'
$relative=$prefix+[Guid]::NewGuid().ToString('N')
$env:SHELL_KEY_PS='HKCU:\'+$relative
$env:SHELL_BACKUP=Join-Path $env:TEST_DIRECTORY 'backup.txt'
function Check([bool]$ok){if(-not $ok){throw 'Registry provider regression failed'}}
function Verify([string]$shell){
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($relative)
    try{
        Check ($key.GetValue('Shell') -eq $shell)
        Check ($key.GetValue('KeepValue') -eq 'preserve-me')
        $child=$key.OpenSubKey('KeepChild');try{Check ($child -and $child.GetValue('Data') -eq 'preserve-child')}finally{if($child){$child.Dispose()}}
    }finally{$key.Dispose()}
}
try{
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($relative)
    try{$key.SetValue('Shell','original-fixture.exe');$key.SetValue('KeepValue','preserve-me');$child=$key.CreateSubKey('KeepChild');try{$child.SetValue('Data','preserve-child')}finally{$child.Dispose()}}finally{$key.Dispose()}
    $env:SHELL_VALUE='first-fixture.exe';. $env:TEST_SOURCE -Action install;Verify 'first-fixture.exe'
    $env:SHELL_VALUE='second-fixture.exe';. $env:TEST_SOURCE -Action install;Verify 'second-fixture.exe'
    Check ([IO.File]::ReadAllText($env:SHELL_BACKUP) -eq 'original-fixture.exe')
    . $env:TEST_SOURCE -Action restore;Verify 'original-fixture.exe'
    . $env:TEST_SOURCE -Action restore;Verify 'original-fixture.exe'
}finally{
    # Check the complete cleanup target before recursive registry deletion.
    if(-not $relative.StartsWith($prefix,[StringComparison]::Ordinal) -or $relative.Substring($prefix.Length) -notmatch '^[0-9a-f]{32}$'){throw 'Unsafe scratch-key cleanup target'}
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($relative,$false)
}
Check (-not (Test-Path -LiteralPath $env:SHELL_KEY_PS))
Write-Output 'PASS: actual HKCU scratch-key provider preserves unrelated values/subkeys and original backup across install/reinstall/restore; scratch leaf removed; Winlogon untouched.'
''',encoding='utf-8')
    env=os.environ.copy();env.update(TEST_DIRECTORY=str(folder),TEST_SOURCE=str(root/'upstream/plasma-windows/tools/shell-registry.ps1'))
    p=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(driver)],env=env,capture_output=True,timeout=30)
    print(p.stdout.decode(errors='replace'));assert p.returncode==0,(p.stdout,p.stderr)
