<#
.SYNOPSIS
    (Re-)apply the qtdeploy.targets MSB4023 fix. Idempotent; safe to run before every build.

.DESCRIPTION
    Qt 6.11's windeployqt prints informational "Skipping system library C:/..." lines to
    STDOUT even in list-target mode. QtMsBuild's qtdeploy.targets redirects that stdout to
    a log, reads every line back as a deployed-file path, and evaluates %(FullPath) on it -
    which throws MSB4023 ("The given path's format is not supported") on the non-path lines,
    failing the GeoDmsGuiQt build at the QtDeploy step.

    The fix drops non-drive-letter lines via a metadata flag before the existing
    !Exists('%(Fullpath)') cleanup. It must be a metadata VALUE compared in a condition:
    calling Regex::IsMatch('%(Identity)') directly inside a Condition is MSB4092.

    Why a script: the patched file lives in %LOCALAPPDATA%\QtMsBuild, which the Qt VS Tools
    extension re-extracts from its package whenever it feels like it - observed after
    extension updates (2026-07-27, 2026-08-08) and after a plain Visual Studio start
    (2026-08-09, package timestamp 2026-04-14 restored, patch gone). Hand-reapplying is a
    losing game, so the .m setup script runs this before msbuild. Exit codes: 0 = patched or
    already patched; 1 = file found but the anchor was not (QtMsBuild changed shape - look);
    2 = QtMsBuild not present at all (no Qt VS Tools on this machine - fine, nothing to do).
#>
[CmdletBinding()]
param(
    [string] $TargetsFile = "$env:LOCALAPPDATA\QtMsBuild\deploy\qtdeploy.targets"
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $TargetsFile)) {
    Write-Host "patch-qtdeploy-targets: $TargetsFile not found - Qt VS Tools not installed, nothing to patch."
    exit 2
}

$text = [IO.File]::ReadAllText($TargetsFile)

if ($text.Contains('<IsPath>')) {
    Write-Host "patch-qtdeploy-targets: already patched."
    exit 0
}

$anchor = @'
    <ItemGroup>
      <QtDeployed Remove="@(QtDeployed)" Condition="!Exists('%(Fullpath)')"/>
    </ItemGroup>
'@
$replacement = @'
    <ItemGroup>
      <QtDeployed>
        <IsPath>$([System.Text.RegularExpressions.Regex]::IsMatch('%(Identity)', '^[A-Za-z]:'))</IsPath>
      </QtDeployed>
      <QtDeployed Remove="@(QtDeployed)" Condition="'%(IsPath)' != 'True'" />
    </ItemGroup>
    <ItemGroup>
      <QtDeployed Remove="@(QtDeployed)" Condition="!Exists('%(Fullpath)')"/>
    </ItemGroup>
'@

# Normalize the anchor's line endings to whatever the file uses before matching.
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$anchor      = $anchor.Replace("`r`n", "`n").Replace("`n", $nl)
$replacement = $replacement.Replace("`r`n", "`n").Replace("`n", $nl)

if (-not $text.Contains($anchor)) {
    Write-Host "patch-qtdeploy-targets: ANCHOR NOT FOUND in $TargetsFile - QtMsBuild layout changed; inspect and update this script."
    exit 1
}

$patched = ([regex]::new([regex]::Escape($anchor))).Replace($text, $replacement.Replace('$', '$$'), 1)

# Refuse to write anything that is not well-formed XML.
$xml = New-Object System.Xml.XmlDocument
$xml.LoadXml($patched)

[IO.File]::WriteAllText($TargetsFile, $patched)
Write-Host "patch-qtdeploy-targets: applied to $TargetsFile."
exit 0
