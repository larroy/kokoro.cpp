#Requires -Version 7
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-RepoRoot {
    Split-Path $PSScriptRoot -Parent
}

function Get-KokoroVersion {
    $cmakeLists = Get-Content -Raw (Join-Path (Get-RepoRoot) 'CMakeLists.txt')
    $match = [regex]::Match($cmakeLists, 'project\(KokoroCPP VERSION (\d+\.\d+\.\d+)')
    if (-not $match.Success) { throw 'project(KokoroCPP VERSION x.y.z) not found in CMakeLists.txt' }
    $match.Groups[1].Value
}

function Get-KokoroRids {
    (Import-PowerShellDataFile (Join-Path $PSScriptRoot 'rids.psd1')).Rows
}

function Get-RuntimePackageId([hashtable]$Row, [hashtable]$Package) {
    "Larroy.Kokoro.runtime.$($Row.Rid)$($Package.Suffix)"
}

# Output goes to the host, never the pipeline, so callers' return values stay clean.
function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

function Get-PeMachine([string]$Path) {
    $buffer = [byte[]]::new(4096)
    $stream = [IO.FileStream]::new((Resolve-Path $Path).Path, 'Open', 'Read')
    try { $read = $stream.Read($buffer, 0, $buffer.Length) } finally { $stream.Dispose() }
    $peOffset = [BitConverter]::ToInt32($buffer, 0x3C)
    if ($peOffset -lt 0 -or $peOffset + 6 -gt $read) { throw "$Path is not a PE file" }
    [int][BitConverter]::ToUInt16($buffer, $peOffset + 4)
}

function Find-VcVarsAll {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vcvars = if (Test-Path $vswhere) {
        & $vswhere -latest -products '*' -find 'VC\Auxiliary\Build\vcvarsall.bat' | Select-Object -First 1
    }
    if (-not $vcvars) { throw 'vcvarsall.bat not found' }
    $vcvars
}

function Get-EnvironmentSnapshot {
    $snapshot = @{}
    Get-ChildItem env: | ForEach-Object { $snapshot[$_.Name] = $_.Value }
    $snapshot
}

function Restore-EnvironmentSnapshot([hashtable]$Snapshot) {
    Get-ChildItem env: | Where-Object { -not $Snapshot.ContainsKey($_.Name) } |
        ForEach-Object { Remove-Item "env:$($_.Name)" }
    foreach ($name in $Snapshot.Keys) {
        if ([Environment]::GetEnvironmentVariable($name) -cne $Snapshot[$name]) {
            [Environment]::SetEnvironmentVariable($name, $Snapshot[$name])
        }
    }
}

function Import-VcEnvironment([string]$VcVars, [string]$Arch) {
    $lines = cmd /c "`"$VcVars`" $Arch >nul && set"
    if ($LASTEXITCODE -ne 0) { throw "vcvarsall.bat $Arch failed with exit code $LASTEXITCODE" }
    foreach ($line in $lines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $separator), $line.Substring($separator + 1))
        }
    }
}

function Invoke-InVcEnvironment([string]$Arch, [scriptblock]$Body) {
    $vcvars = Find-VcVarsAll
    $snapshot = Get-EnvironmentSnapshot
    try {
        Import-VcEnvironment $vcvars $Arch
        & $Body
    } finally {
        Restore-EnvironmentSnapshot $snapshot
    }
}

function New-Nuspec([string]$Template, [hashtable]$Tokens, [string]$Destination) {
    $content = Get-Content -Raw $Template
    foreach ($key in $Tokens.Keys) { $content = $content.Replace("@$key@", [string]$Tokens[$key]) }
    $leftover = [regex]::Match($content, '@[A-Z_]+@')
    if ($leftover.Success) { throw "Unreplaced token $($leftover.Value) in $Template" }
    [IO.File]::WriteAllText($Destination, $content, [Text.UTF8Encoding]::new($false))
}

Export-ModuleMember -Function Get-RepoRoot, Get-KokoroVersion, Get-KokoroRids, Get-RuntimePackageId, Invoke-Checked,
    Get-PeMachine, Invoke-InVcEnvironment, New-Nuspec
