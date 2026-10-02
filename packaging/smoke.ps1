#Requires -Version 7
<#
.SYNOPSIS
End-to-end check of the packages in a local feed: a fresh console app that references only Larroy.Kokoro
synthesizes audio, and a win-arm64 build of it carries arm64 natives only.
#>
param(
    [string]$Framework = 'net8.0',
    [string]$Feed = 'artifacts/nuget',
    [string]$Model = 'models/kokoro-v1.1-zh.onnx',
    [string]$Voices = 'models/voices-v1.1-zh.bin',
    [string]$Dict = 'dict'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Kokoro.Packaging.psm1') -Force

$root = Split-Path $PSScriptRoot -Parent
$arm64Machine = 0xAA64

function Resolve-RepoPath([string]$Path) {
    [IO.Path]::GetFullPath($Path, $root)
}

function Set-TargetFramework([string]$Project) {
    # SDK 10's console template only offers its own TFM, so retarget the generated project.
    $content = (Get-Content -Raw $Project) -replace '<TargetFramework>[^<]+</TargetFramework>',
        "<TargetFramework>$Framework</TargetFramework>"
    Set-Content -Path $Project -Value $content -NoNewline
}

function New-SmokeApp([string]$Work, [string]$Version) {
    $app = Join-Path $Work 'app'
    $project = Join-Path $app 'KokoroSmoke.csproj'
    Invoke-Checked dotnet @('new', 'console', '-n', 'KokoroSmoke', '-o', $app)
    Set-TargetFramework $project
    Invoke-Checked dotnet @('new', 'nugetconfig', '-o', $app)
    Invoke-Checked dotnet @('nuget', 'add', 'source', (Resolve-RepoPath $Feed), '-n', 'kokoro-local',
        '--configfile', (Join-Path $app 'nuget.config'))
    Invoke-Checked dotnet @('add', $project, 'package', 'Larroy.Kokoro', '--version', $Version)
    Copy-Item (Join-Path $PSScriptRoot 'smoke/Program.cs') (Join-Path $app 'Program.cs') -Force
    $app
}

function Invoke-Synthesis([string]$App, [string]$Version) {
    Invoke-Checked dotnet @('run', '--project', $App, '-c', 'Release', '--',
        (Resolve-RepoPath $Model), (Resolve-RepoPath $Voices), (Resolve-RepoPath $Dict), $Version)
}

function Assert-Arm64([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing native in win-arm64 output: $Path" }
    $machine = Get-PeMachine $Path
    if ($machine -ne $arm64Machine) { throw ('{0} is machine 0x{1:X}, win-arm64 needs 0xAA64' -f $Path, $machine) }
}

function Assert-Arm64Build([string]$App, [string]$Work) {
    $out = Join-Path $Work 'arm64'
    Invoke-Checked dotnet @('build', $App, '-c', 'Release', '-r', 'win-arm64', '-o', $out)
    Assert-Arm64 (Join-Path $out 'kokoro.dll')
    Assert-Arm64 (Join-Path $out 'onnxruntime.dll')
    Get-ChildItem $out -Recurse -Filter 'kokoro.dll' | ForEach-Object { Assert-Arm64 $_.FullName }
    $x64Natives = Join-Path $out 'runtimes/win-x64'
    if (Test-Path $x64Natives) { throw "win-arm64 output contains x64 natives: $x64Natives" }
}

$version = Get-KokoroVersion
$work = Join-Path ([IO.Path]::GetTempPath()) "kokoro-smoke-$([guid]::NewGuid().ToString('N'))"
$savedPackages = $env:NUGET_PACKAGES
try {
    $env:NUGET_PACKAGES = Join-Path $work 'packages'
    $app = New-SmokeApp $work $version
    Invoke-Synthesis $app $version
    Assert-Arm64Build $app $work
    Write-Host "SMOKE OK $version"
} finally {
    $env:NUGET_PACKAGES = $savedPackages
    if (Test-Path $work) { Remove-Item $work -Recurse -Force }
}
