param([string]$CraftRoot = 'D:\KDE\CraftRoot', [string]$Blueprints = 'D:\KDE\blueprints')
$ErrorActionPreference = 'Stop'
$proxyUri = [System.Net.WebRequest]::DefaultWebProxy.GetProxy([uri]'https://github.com')
if ($proxyUri.Host -ne 'github.com') {
    $env:https_proxy = $proxyUri.AbsoluteUri
    $env:http_proxy = $proxyUri.AbsoluteUri
}
$env:no_proxy = 'localhost,127.0.0.1,.kde.org,.msys2.org,mirror.twds.com.tw'
$settings = Join-Path $CraftRoot 'etc\CraftSettings.ini'
if (-not (Test-Path -LiteralPath (Join-Path $CraftRoot 'craft\bin\craft.py'))) {
    throw 'Craft bootstrap must complete before building the port.'
}
& python (Join-Path $PSScriptRoot 'configure-port.py') --blueprints $Blueprints --craft (Join-Path $CraftRoot 'craft')
if ($LASTEXITCODE) { throw 'Blueprint configuration failed' }
$content = [IO.File]::ReadAllText($settings)
$content = $content.Replace('EnableDailyUpdates = True', 'EnableDailyUpdates = False')
$content = $content -replace '(?m)^#?Jobs\s*=.*$', 'Jobs = 8'
$content = $content -replace '(?m)^RepositoryUrl\s*=.*$', 'RepositoryUrl = https://mirror.twds.com.tw/kde-application/craft/Qt6/;https://files.kde.org/craft/Qt6/'
if ($content -match '(?m)^CacheVersion\s*=') {
    $content = $content -replace '(?m)^CacheVersion\s*=.*$', 'CacheVersion = 26.05'
} else {
    $content = $content.Replace('[Packager]', "[Packager]`nCacheVersion = 26.05")
}
$content = $content -replace '(?m)^#Locations = .*$', "Locations = $Blueprints"
# Keep Craft from discovering a second, unpatched checkout of the same recipes.
$content = $content -replace '(?m)^#BlueprintRoot = .*$', "BlueprintRoot = $CraftRoot/etc/port-blueprint-locations"
[IO.File]::WriteAllText($settings, $content, [Text.UTF8Encoding]::new($false))
$env:CRAFT_ROOT = $CraftRoot
$script:PythonExe = (Get-Command python).Source
$script:CraftScript = Join-Path $CraftRoot 'craft\bin\craft.py'
$setup = & $script:PythonExe (Join-Path $CraftRoot 'craft\bin\CraftSetupHelper.py') --setup --format=json
if ($LASTEXITCODE) { throw 'Craft environment setup failed' }
$setupEnv = ($setup -join "`n") | ConvertFrom-Json
foreach ($property in $setupEnv.PSObject.Properties) {
    Set-Item -LiteralPath "Env:$($property.Name)" -Value ([string]$property.Value)
}
function craft { & $script:PythonExe $script:CraftScript @args }
$logDir = Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
foreach ($package in 'kconfig', 'kcoreaddons', 'kwindowsystem', 'kauth', 'kded', 'kiconthemes', 'kio', 'krunner', 'plasma-activities-stats', 'libplasma', 'plasma5support', 'plasma-workspace', 'plasma-desktop', 'kactivitymanagerd', 'dolphin') {
    Write-Host "Building $package"
    Write-Host "Stage deps $package"
    & craft --install-deps $package *> (Join-Path $logDir "$package-deps.log")
    if ($LASTEXITCODE) { throw "Dependency build failed: $package (see logs)" }
    if ($package -eq 'plasma-workspace') {
        $interfaceBuild = 'D:\KDE\interface-build'
        & cmake -S (Join-Path $PSScriptRoot 'interfaces') -B $interfaceBuild -G Ninja "-DCMAKE_PREFIX_PATH=$CraftRoot" "-DCMAKE_INSTALL_PREFIX=$CraftRoot" '-DKWIN_SOURCE=D:/KDE/sources/extra/kwin-6.7.4' '-DLOCKER_SOURCE=D:/KDE/sources/extra/kscreenlocker-6.7.4' '-DSTATUS_NOTIFIER_SOURCE=D:/KDE/sources/extra/kstatusnotifieritem-6.28.0'
        if ($LASTEXITCODE) { throw 'D-Bus interface configuration failed' }
        & cmake --build $interfaceBuild --target install
        if ($LASTEXITCODE) { throw 'D-Bus interface generation failed' }
    }
    Write-Host "Stage build $package"
    & craft --no-cache $package *> (Join-Path $logDir "$package-build.log")
    if ($LASTEXITCODE) { throw "Build failed: $package (see logs)" }
}
& cmake -S (Join-Path $PSScriptRoot 'native') -B 'D:\KDE\native-build' -G Ninja "-DCMAKE_PREFIX_PATH=$CraftRoot" "-DCMAKE_INSTALL_PREFIX=$CraftRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_PLASMA_MODULES=ON -DBUILD_VM_PROBES=ON
if ($LASTEXITCODE) { throw 'Native tray bridge configuration failed' }
& cmake --build 'D:\KDE\native-build' --target install --parallel 8
if ($LASTEXITCODE) { throw 'Native tray bridge build failed' }
& (Join-Path $PSScriptRoot 'package-port.ps1') -CraftRoot $CraftRoot
