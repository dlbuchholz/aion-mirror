<#
.SYNOPSIS
  Build, test and run Liora with MSVC on Windows (Visual Studio generator, build-vs folder).

.DESCRIPTION
  Configures the build folder the first time, then builds with all cores and runs the tests in
  parallel. Needs Visual Studio 2022 or its Build Tools, CMake, and Qt's msvc2022_64 kit.

  The Qt kit is, in this order: -QtDir, the LIORA_QT_DIR environment variable, the kit the build
  folder was configured with, or the newest msvc*_64 kit under C:\Qt or D:\Qt.

.EXAMPLE
  .\scripts\build.ps1                      # build
  .\scripts\build.ps1 run                  # build, then start Liora
  .\scripts\build.ps1 test                 # build, then run the tests
  .\scripts\build.ps1 test run             # build, test, start Liora only if the tests pass
  .\scripts\build.ps1 all                  # same as "test run"
  .\scripts\build.ps1 clean build          # wipe build-vs, configure and build from scratch
  .\scripts\build.ps1 configure            # re-run CMake (new source files, git hash for the About page)
  .\scripts\build.ps1 run -Config Release  # any action, other configuration
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [ValidateSet('build', 'test', 'run', 'all', 'configure', 'clean')]
    [string[]] $Actions = @('build'),

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string] $Config = 'Debug',

    # Qt kit, for example C:\Qt\6.12.0\msvc2022_64
    [string] $QtDir = $env:LIORA_QT_DIR,

    # How many files to compile and tests to run at once
    [int] $Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Repo 'build-vs'
$Cache = Join-Path $BuildDir 'CMakeCache.txt'

function Format-Duration([TimeSpan] $Span) {
    if ($Span.TotalMinutes -ge 1) { '{0}m {1:00.0}s' -f [int][math]::Floor($Span.TotalMinutes), ($Span.TotalSeconds % 60) }
    else { '{0:0.0}s' -f $Span.TotalSeconds }
}

function Invoke-Step([string] $Title, [scriptblock] $Command) {
    Write-Host "==> $Title" -ForegroundColor Cyan
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $global:LASTEXITCODE = 0
    & $Command
    $watch.Stop()
    $took = Format-Duration $watch.Elapsed
    if ($LASTEXITCODE -ne 0) { throw "$Title failed after $took (exit code $LASTEXITCODE)" }
    Write-Host "    $Title took $took" -ForegroundColor DarkGray
}

# The kit the build folder was configured with, else the newest MSVC kit in the usual places
function Find-QtDir {
    if (Test-Path $Cache) {
        $line = Select-String -Path $Cache -Pattern '^CMAKE_PREFIX_PATH:[A-Z]+=(.+)$' | Select-Object -First 1
        if ($line) { return $line.Matches[0].Groups[1].Value.Replace('/', '\') }
    }
    $kits = foreach ($root in 'C:\Qt', 'D:\Qt') {
        Get-ChildItem -Path "$root\6.*\msvc*_64" -Directory -ErrorAction SilentlyContinue
    }
    $kits | Sort-Object { [version]$_.Parent.Name } -Descending | Select-Object -First 1 -ExpandProperty FullName
}

$total = [Diagnostics.Stopwatch]::StartNew()

if (-not $QtDir) { $QtDir = Find-QtDir }
if (-not $QtDir) { throw "No Qt MSVC kit found under C:\Qt or D:\Qt - pass -QtDir or set LIORA_QT_DIR" }

# Qt's DLLs for the tests and the app. Prepended, so no other Qt or Cygwin DLL wins.
$qtBin = Join-Path $QtDir 'bin'
if (-not (Test-Path $qtBin)) { throw "Qt not found at $qtBin - pass -QtDir or set LIORA_QT_DIR" }
if (($env:Path -split ';') -notcontains $qtBin) { $env:Path = "$qtBin;$env:Path" }

if ($Actions -contains 'all') { $Actions += 'test', 'run' }
$needsBuild = [bool]($Actions | Where-Object { $_ -in 'build', 'test', 'run' })

Push-Location $Repo
try {
    if ($Actions -contains 'clean' -and (Test-Path $BuildDir)) {
        Write-Host "==> Removing $BuildDir" -ForegroundColor Cyan
        Remove-Item -Recurse -Force $BuildDir
    }

    # The first time CMake picks the newest Visual Studio; later it keeps the folder's generator
    if ($Actions -contains 'configure' -or ($needsBuild -and -not (Test-Path $Cache))) {
        $prefix = $QtDir.Replace('\', '/')
        Invoke-Step "Configure ($QtDir)" { cmake -S $Repo -B $BuildDir -A x64 "-DCMAKE_PREFIX_PATH=$prefix" }
    }

    if ($needsBuild) {
        Invoke-Step "Build ($Config)" { cmake --build $BuildDir --config $Config --parallel $Jobs }
    }

    # Off screen, the tests' windows don't appear and can't take the focus from each other
    if ($Actions -contains 'test') {
        $env:QT_QPA_PLATFORM = 'offscreen'
        try {
            Invoke-Step "Test ($Config)" { ctest --test-dir $BuildDir -C $Config --output-on-failure -j $Jobs }
        }
        finally {
            Remove-Item Env:QT_QPA_PLATFORM
        }
    }

    $total.Stop()
    Write-Host "==> Done in $(Format-Duration $total.Elapsed)" -ForegroundColor Green

    if ($Actions -contains 'run') {
        $exe = Join-Path $BuildDir "$Config\Liora.exe"
        Write-Host "==> Starting $exe" -ForegroundColor Cyan
        & $exe
    }
}
finally {
    Pop-Location
}
