# SPDX-License-Identifier: GPL-3.0-or-later
# Environment paths are data, never interpolated into PowerShell source or CMD echo.
$ErrorActionPreference = 'Stop'
if (-not $env:CRAFT_DATA -or -not $env:LOCALAPPDATA) {
    throw 'CRAFT_DATA and LOCALAPPDATA are required to write the session menu.'
}
$directory = Join-Path $env:LOCALAPPDATA 'menus'
[IO.Directory]::CreateDirectory($directory) | Out-Null
$settings = [Xml.XmlWriterSettings]::new()
$settings.Encoding = [Text.UTF8Encoding]::new($false)
$settings.Indent = $true
$writer = [Xml.XmlWriter]::Create((Join-Path $directory 'applications.menu'), $settings)
try {
    $writer.WriteStartElement('Menu')
    $writer.WriteElementString('Name', 'Applications')
    $writer.WriteElementString('Directory', 'applications.directory')
    foreach ($path in @((Join-Path $env:LOCALAPPDATA 'applications'), (Join-Path $env:CRAFT_DATA 'applications'))) {
        $writer.WriteElementString('AppDir', $path.Replace('\', '/'))
    }
    $writer.WriteElementString('DefaultDirectoryDirs', '')
    $writer.WriteElementString('DefaultMergeDirs', '')
    $writer.WriteStartElement('Include')
    $writer.WriteElementString('All', '')
    $writer.WriteEndElement()
    $writer.WriteEndElement()
} finally {
    $writer.Dispose()
}
