#Requires -Version 7
<#
.SYNOPSIS
Packs Larroy.Kokoro and its runtime packages from artifacts/natives/<rid>/ into artifacts/nuget/.
#>
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Kokoro.Packaging.psm1') -Force

$root = Split-Path $PSScriptRoot -Parent
$feed = Join-Path $root 'artifacts/nuget'
$nuspecDir = Join-Path $root 'artifacts/obj/nuspec'
$readme = Join-Path $PSScriptRoot 'README.md'
$licenseFiles = @('LICENSE', 'THIRD_PARTY_NOTICES.md')

function Reset-Directory([string]$Path) {
    if (Test-Path $Path) { Remove-Item $Path -Recurse -Force }
    New-Item -ItemType Directory -Path $Path | Out-Null
}

function Get-NativeSource([hashtable]$Row, [string]$RelativePath) {
    $path = [IO.Path]::GetFullPath((Join-Path $root "artifacts/natives/$($Row.Rid)/$RelativePath"))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing native file: $path" }
    $path
}

function Assert-NativesStaged([object[]]$Rows) {
    foreach ($row in $Rows) {
        foreach ($file in @($row.Packages.Files)) { Get-NativeSource $row $file | Out-Null }
    }
}

function Get-BuildTransitiveEntries([hashtable]$Package, [string]$Id) {
    if (-not $Package.BuildTransitive) { return }
    $src = Join-Path $PSScriptRoot $Package.BuildTransitive
    if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { throw "Missing build file: $src" }
    "    <file src=`"$src`" target=`"buildTransitive/$Id.targets`" />"
}

function Get-LicenseEntries {
    foreach ($name in $licenseFiles) {
        $src = Join-Path $root $name
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { throw "Missing license file: $src" }
        "    <file src=`"$src`" target=`"$name`" />"
    }
}

function Get-FileEntries([hashtable]$Row, [hashtable]$Package, [string]$Id) {
    $natives = @($Package.Files | ForEach-Object {
            "    <file src=`"$(Get-NativeSource $Row $_)`" target=`"runtimes/$($Row.Rid)/native/$_`" />"
        })
    (@($natives) + @(Get-BuildTransitiveEntries $Package $Id) + @(Get-LicenseEntries)) -join "`n"
}

function Get-DependencyEntries([hashtable]$Package, [string]$OrtVersion) {
    # A bare version is an inclusive minimum, so NuGet unifies with the app's own, newer ONNX Runtime.
    ($Package.Dependencies | ForEach-Object { "      <dependency id=`"$_`" version=`"$OrtVersion`" />" }) -join "`n"
}

function Invoke-NuspecPack([string]$Nuspec) {
    Invoke-Checked nuget @('pack', $Nuspec, '-OutputDirectory', $feed, '-NonInteractive')
}

function New-RuntimePackage([hashtable]$Row, [hashtable]$Package, [string]$Version, [string]$OrtVersion) {
    $id = Get-RuntimePackageId $Row $Package
    $nuspec = Join-Path $nuspecDir "$id.nuspec"
    $tokens = @{
        ID = $id; VERSION = $Version; DESCRIPTION = $Package.Description; README = $readme
        DEPENDENCIES = Get-DependencyEntries $Package $OrtVersion; FILES = Get-FileEntries $Row $Package $id
    }
    New-Nuspec (Join-Path $PSScriptRoot 'runtime.nuspec.in') $tokens $nuspec
    Invoke-NuspecPack $nuspec
}

function Get-BasePackageIds([object[]]$Rows) {
    foreach ($row in $Rows) {
        $row.Packages | Where-Object { $_.Suffix -eq '' } | ForEach-Object { Get-RuntimePackageId $row $_ }
    }
}

function New-WrapperPackage([object[]]$Rows, [string]$Version) {
    $dependencies = (Get-BasePackageIds $Rows | ForEach-Object {
            "        <dependency id=`"$_`" version=`"[$Version]`" />"
        }) -join "`n"
    $nuspec = Join-Path $nuspecDir 'Larroy.Kokoro.nuspec'
    $tokens = @{
        VERSION = $Version; README = $readme; DEPENDENCIES = $dependencies
        BIN = Join-Path $root 'dotnet/src/Kokoro.Net/bin/Release'
        LICENSES = (Get-LicenseEntries) -join "`n"
        DICT = Join-Path $root 'dict'
        TARGETS = Join-Path $PSScriptRoot 'Larroy.Kokoro.targets'
    }
    New-Nuspec (Join-Path $PSScriptRoot 'Larroy.Kokoro.nuspec.in') $tokens $nuspec
    $project = Join-Path $root 'dotnet/src/Kokoro.Net/Kokoro.Net.csproj'
    Invoke-Checked dotnet @('pack', $project, '-c', 'Release', '-o', $feed, "-p:NuspecFile=$nuspec")
}

function Assert-PackageSet([object[]]$Rows, [string]$Version) {
    $runtimeIds = foreach ($row in $Rows) { $row.Packages | ForEach-Object { Get-RuntimePackageId $row $_ } }
    $expected = (@('Larroy.Kokoro') + @($runtimeIds) | ForEach-Object { "$_.$Version.nupkg" } | Sort-Object) -join ', '
    $actual = (Get-ChildItem $feed -Filter '*.nupkg' | ForEach-Object Name | Sort-Object) -join ', '
    if ($actual -ne $expected) { throw "Unexpected package set: $actual (expected $expected)" }
    Write-Host "Packed $actual"
}

if (-not (Get-Command nuget -ErrorAction SilentlyContinue)) {
    throw 'nuget.exe not found on PATH; install it with `winget install Microsoft.NuGet` (CI: NuGet/setup-nuget)'
}
$version = Get-KokoroVersion
$ortVersion = Get-OrtVersion
$rows = Get-KokoroRids
Reset-Directory $feed
Reset-Directory $nuspecDir
Assert-NativesStaged $rows
foreach ($row in $rows) {
    foreach ($package in $row.Packages) { New-RuntimePackage $row $package $version $ortVersion }
}
New-WrapperPackage $rows $version
Assert-PackageSet $rows $version
