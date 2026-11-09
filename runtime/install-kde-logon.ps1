# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([ValidateSet('Install','Restore')][string]$Action='Install',[switch]$Plan)
$ErrorActionPreference='Stop'
$id='{2B2E39BE-4E8A-47AA-9FC1-E388F34F2601}'
$source=Join-Path $PSScriptRoot 'bin\kde-logon-provider.dll'
$protectedRoot=Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'KDE-on-Win'
$target=Join-Path $protectedRoot 'Logon\kde-logon-provider.dll'
$class='Software\Classes\CLSID\'+$id
$provider='Software\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\'+$id
if($Plan){[pscustomobject]@{Action=$Action;Source=$source;ProtectedTarget=$target;Class=$class;Provider=$provider;FiltersModified=$false;SystemProvidersDisabled=$false;DefaultProviderChanged=$false}|ConvertTo-Json;return}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if(-not ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Run this installer as administrator in the test VM.'}
$registry=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,[Microsoft.Win32.RegistryView]::Registry64)
try{
    if($Action -eq 'Restore'){
        $key=$registry.OpenSubKey($class+'\InprocServer32')
        $owned=$false
        if($key){try{$owned=$key.GetValue('') -ieq $target}finally{$key.Dispose()}}
        if(-not $owned){throw 'Registration is absent or no longer owned by KDE-on-Win; nothing removed.'}
        $registry.DeleteSubKeyTree($provider,$false)
        $registry.DeleteSubKeyTree($class,$false)
        Write-Output 'KDE logon tile unregistered. Windows password/PIN/Hello providers were not changed.'
        return
    }
    if(-not [Environment]::Is64BitOperatingSystem){throw 'The experimental provider requires x64 Windows.'}
    if(-not (Test-Path -LiteralPath $source -PathType Leaf)){throw "Missing provider: $source"}
    $directory=Split-Path -Parent $target
    [IO.Directory]::CreateDirectory($protectedRoot)|Out-Null
    [IO.Directory]::CreateDirectory($directory)|Out-Null
    foreach($path in @($protectedRoot,$directory,$target)){if((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Refusing reparse point: $path"}}
    # LogonUI runs as SYSTEM. Never register a DLL from user-writable CraftRoot.
    $acl=[Security.AccessControl.DirectorySecurity]::new()
    $acl.SetOwner([Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))
    $acl.SetAccessRuleProtection($true,$false)
    foreach($entry in @(@('S-1-5-18','FullControl'),@('S-1-5-32-544','FullControl'),@('S-1-5-32-545','ReadAndExecute'))){
        $sid=[Security.Principal.SecurityIdentifier]::new($entry[0])
        $rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,[Security.AccessControl.FileSystemRights]$entry[1],[Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit',[Security.AccessControl.PropagationFlags]::None,[Security.AccessControl.AccessControlType]::Allow)
        $acl.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $protectedRoot -AclObject $acl
    Set-Acl -LiteralPath $directory -AclObject $acl
    Copy-Item -LiteralPath $source -Destination $target -Force
    $fileAcl=[Security.AccessControl.FileSecurity]::new()
    $fileAcl.SetOwner([Security.Principal.SecurityIdentifier]::new('S-1-5-32-544'))
    $fileAcl.SetAccessRuleProtection($true,$false)
    foreach($entry in @(@('S-1-5-18','FullControl'),@('S-1-5-32-544','FullControl'),@('S-1-5-32-545','ReadAndExecute'))){
        $rule=[Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($entry[0]),[Security.AccessControl.FileSystemRights]$entry[1],[Security.AccessControl.AccessControlType]::Allow)
        $fileAcl.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $target -AclObject $fileAcl
    $key=$registry.CreateSubKey($class+'\InprocServer32')
    try{$key.SetValue('',$target);$key.SetValue('ThreadingModel','Apartment')}finally{$key.Dispose()}
    $key=$registry.CreateSubKey($provider)
    try{$key.SetValue('','KDE-on-Win Local Account (Experimental)')}finally{$key.Dispose()}
    Write-Output 'Experimental local-account KDE tile registered. Select it under Sign-in options. System providers remain available.'
}finally{$registry.Dispose()}
