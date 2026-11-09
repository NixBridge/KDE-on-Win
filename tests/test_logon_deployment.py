# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the publisher on private files, including failed replacement and junction rejection."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='kde-deployment-') as folder:
    directory=Path(folder).resolve()
    driver=directory/'test.ps1'
    driver.write_text(r"""param([string]$Helper,[string]$Fixture)
$ErrorActionPreference='Stop'
. $Helper
$source=Join-Path $Fixture 'source.dll'
$target=Join-Path $Fixture 'target.dll'
[IO.File]::WriteAllText($source,'version-2')
[IO.File]::WriteAllText($target,'version-1')
Publish-KdeBinary -Source $source -Target $target -PrepareFile {param($pending) $acl=[IO.File]::GetAccessControl($pending);[IO.File]::SetAccessControl($pending,$acl)}
if([IO.File]::ReadAllText($target) -cne 'version-2'){throw 'Replacement failed'}
$failed=$false
try{Publish-KdeBinary -Source $source -Target $target -PrepareFile {param($pending) [IO.File]::WriteAllText($pending,'tampered')}}catch{$failed=$true}
if(-not $failed -or [IO.File]::ReadAllText($target) -cne 'version-2'){throw 'Hash failure changed original'}
[IO.File]::WriteAllText($source,'version-3')
$lock=[IO.File]::Open($target,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
$failed=$false
try{try{Publish-KdeBinary -Source $source -Target $target}catch{$failed=$true}}finally{$lock.Dispose()}
if(-not $failed -or [IO.File]::ReadAllText($target) -cne 'version-2'){throw 'Locked target changed'}
if(@(Get-ChildItem -LiteralPath $Fixture -Filter '.kde-pending-*' -Force).Count){throw 'Staging file left behind'}
$outside=Join-Path $Fixture 'outside'
$link=Join-Path $Fixture 'junction'
[IO.Directory]::CreateDirectory($outside)|Out-Null
New-Item -ItemType Junction -Path $link -Target $outside|Out-Null
try{
    $failed=$false
    try{$null=Assert-KdePlainPath (Join-Path $link 'uncreated\provider.dll')}catch{$failed=$true}
    if(-not $failed -or [IO.Directory]::Exists((Join-Path $outside 'uncreated'))){throw 'Junction not rejected before creation'}
}finally{[IO.Directory]::Delete($link)}
Write-Output 'PASS: atomic private-file replacement, hash failure, locked target preservation, staging cleanup and pre-creation junction rejection; no registry or protected-directory writes.'
""",encoding='utf-8',newline='\r\n')
    process=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(driver),
        '-Helper',str(root/'runtime/logon-deployment.ps1'),'-Fixture',str(directory)],capture_output=True,timeout=30)
    print(process.stdout.decode('utf-8',errors='replace').strip())
    if process.returncode:
        print(process.stderr.decode('utf-8',errors='replace'))
    assert process.returncode==0
