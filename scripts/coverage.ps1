<#
.SYNOPSIS
    Measures test coverage of src/ with OpenCppCoverage (Windows, MSVC debug build).

.DESCRIPTION
    Runs every test registered with CTest in the build folder under OpenCppCoverage,
    merges the results and writes:
      <BuildDir>\coverage\html\index.html   browsable report
      <BuildDir>\coverage\coverage.xml      Cobertura XML (for CI tools)
    and prints line coverage per source file.

    Requires OpenCppCoverage (winget install OpenCppCoverage.OpenCppCoverage) and a
    Debug build (needs the .pdb files).

.EXAMPLE
    .\scripts\coverage.ps1 -BuildDir build\Desktop_Qt_6_12_0_MSVC2022_64bit_Debug -QtBin D:\Qt\6.12.0\msvc2022_64\bin
#>
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$QtBin = "",                             # Folder with the Qt DLLs, if not on PATH
    [string]$OpenCppCoverage = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$BuildDir = (Resolve-Path $BuildDir).Path
$sources = Join-Path $repo "src"
$output = Join-Path $BuildDir "coverage"

if (-not (Test-Path $OpenCppCoverage)) {
    throw "OpenCppCoverage not found at '$OpenCppCoverage'. Install it with: winget install OpenCppCoverage.OpenCppCoverage"
}
if ($QtBin) {
    $env:PATH = "$QtBin;$env:PATH"
}

# ctest from PATH, otherwise next to the cmake that configured this build folder
$ctest = (Get-Command ctest -ErrorAction SilentlyContinue).Source
if (-not $ctest) {
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    $cmakeLine = if (Test-Path $cache) { Select-String -Path $cache -Pattern '^CMAKE_COMMAND:INTERNAL=(.+)$' | Select-Object -First 1 }
    if ($cmakeLine) {
        $ctest = Join-Path (Split-Path -Parent $cmakeLine.Matches[0].Groups[1].Value) "ctest.exe"
    }
}
if (-not $ctest -or -not (Test-Path $ctest)) {
    throw "ctest not found. Add CMake's bin folder to PATH or check that '$BuildDir' is a configured build folder."
}

# Only the tests CTest knows about (old test executables may still lie around)
Push-Location $BuildDir
$tests = & $ctest -N |Select-String -Pattern "Test\s+#\d+:\s+(\S+)" | ForEach-Object { $_.Matches[0].Groups[1].Value }
Pop-Location
if (-not $tests) {
    throw "No tests found in '$BuildDir'. Build the project (including tests) first."
}

Remove-Item $output -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $output | Out-Null

$partials = @()
$failed = @()
foreach ($test in $tests) {
    $exe = Join-Path $BuildDir "$test.exe"
    $partial = Join-Path $output "$test.cov"
    Write-Host "Running $test"
    & $OpenCppCoverage --quiet --sources $sources --export_type "binary:$partial" -- $exe *> $null
    if ($LASTEXITCODE -ne 0) { $failed += $test }
    $partials += $partial
}

# Merge all runs into one report
$inputs = @()
foreach ($partial in $partials) { $inputs += @("--input_coverage", $partial) }
& $OpenCppCoverage --quiet @inputs --export_type "html:$(Join-Path $output 'html')" --export_type "cobertura:$(Join-Path $output 'coverage.xml')" *> $null
Remove-Item $partials -ErrorAction SilentlyContinue

# Summary per file, least covered first. Every test executable is its own module with its own
# copy of the library code, so merge by file and line: a line counts if any test ran it.
[xml]$report = Get-Content (Join-Path $output "coverage.xml")
$lineHits = @{}                                      # file -> @{ line number -> covered }
foreach ($class in $report.coverage.packages.package.classes.class) {
    $file = $class.filename -replace '^.*?[\\/]src[\\/]', 'src/' -replace '\\', '/'
    if (-not $lineHits.ContainsKey($file)) { $lineHits[$file] = @{} }
    foreach ($line in @($class.lines.line)) {
        $number = [int]$line.number
        $lineHits[$file][$number] = $lineHits[$file][$number] -or ([int]$line.hits -gt 0)
    }
}
$files = foreach ($file in $lineHits.Keys) {
    $lines = $lineHits[$file].Values
    $covered = @($lines | Where-Object { $_ }).Count
    [pscustomobject]@{
        File     = $file
        Lines    = $lines.Count
        Covered  = $covered
        Coverage = if ($lines.Count) { [math]::Round(100.0 * $covered / $lines.Count) } else { 100 }
    }
}
$files | Sort-Object Coverage, File | Format-Table -AutoSize

$total = ($files | Measure-Object Lines -Sum).Sum
$coveredTotal = ($files | Measure-Object Covered -Sum).Sum
Write-Host ("Total: {0} of {1} lines covered ({2}%)" -f $coveredTotal, $total, [math]::Round(100.0 * $coveredTotal / $total))
Write-Host "Report: $(Join-Path $output 'html\index.html')"
if ($failed) {
    Write-Warning "These tests failed while measuring: $($failed -join ', ')"
}
