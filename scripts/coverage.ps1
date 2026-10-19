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

    With -Publish it also writes COVERAGE.md (per file: coverage, the test suites that run it and
    links to the uncovered lines) and docs/coverage-badge.svg, which the README shows.

.EXAMPLE
    .\scripts\coverage.ps1 -BuildDir build\Desktop_Qt_6_12_0_MSVC2022_64bit_Debug -QtBin D:\Qt\6.12.0\msvc2022_64\bin

.EXAMPLE
    .\scripts\coverage.ps1 -BuildDir build\Desktop_Qt_6_12_0_MSVC2022_64bit_Debug -QtBin D:\Qt\6.12.0\msvc2022_64\bin -Publish
#>
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$QtBin = "",                             # Folder with the Qt DLLs, if not on PATH
    [string]$OpenCppCoverage = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe",
    [switch]$Publish                                 # Also write COVERAGE.md and docs/coverage-badge.svg for the repository
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
$suiteFiles = @{}                                    # test -> files it runs code of
$suiteCases = @{}                                    # test -> names of its test functions
foreach ($test in $tests) {
    $exe = Join-Path $BuildDir "$test.exe"
    $partial = Join-Path $output "$test.cov"
    $suiteXml = Join-Path $output "$test.xml"
    Write-Host "Running $test"
    & $OpenCppCoverage --quiet --sources $sources --export_type "binary:$partial" --export_type "cobertura:$suiteXml" -- $exe *> $null
    if ($LASTEXITCODE -ne 0) { $failed += $test }
    $partials += $partial

    if ($Publish) {
        # Which files this suite runs, and how many test functions it has (QtTest lists them with -functions).
        # Every test program runs each file's global constants at startup, so a suite only counts for a
        # file once it runs a real part of it: 5 lines, or a quarter of a small file.
        [xml]$suiteReport = Get-Content $suiteXml
        $suiteFiles[$test] = @($suiteReport.coverage.packages.package.classes.class |
            Where-Object {
                $lines = @($_.lines.line)
                $hit = @($lines | Where-Object { [int]$_.hits -gt 0 }).Count
                $hit -gt 0 -and $hit -ge [math]::Min(5, [math]::Ceiling($lines.Count / 4))
            } |
            ForEach-Object { $_.filename -replace '^.*?[\\/]src[\\/]', 'src/' -replace '\\', '/' } | Sort-Object -Unique)
        $suiteCases[$test] = @(& $exe -functions 2>$null | ForEach-Object { $_.Trim() } |
            Where-Object { $_ -match '\(\)$' -and $_ -notmatch '^(initTestCase|cleanupTestCase|init|cleanup)\(\)$' } |
            ForEach-Object { $_ -replace '\(\)$', '' })
    }
    Remove-Item $suiteXml -ErrorAction SilentlyContinue
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

if (-not $Publish) {
    return
}

# --- COVERAGE.md and the badge, for the repository ---

$percent = [math]::Round(100.0 * $coveredTotal / $total)
$commit = (& git -C $repo rev-parse --short HEAD 2>$null)
$dirty = (& git -C $repo status --porcelain --untracked-files=no 2>$null)
$measuredAt = if ($commit) { "``$commit``" + $(if ($dirty) { " (modified)" } else { "" }) } else { "unknown commit" }
$dot = " $([char]0x00B7) "                          # Middle dot, written by code so this file stays ASCII

# Uncovered lines as ranges: "33-40, 57"
function Get-UncoveredRanges($hits) {
    $missing = @($hits.Keys | Where-Object { -not $hits[$_] } | Sort-Object)
    $ranges = @()
    $start = $null
    $previous = $null
    foreach ($line in $missing + @($null)) {
        if ($null -ne $start -and ($null -eq $line -or $line -ne $previous + 1)) {
            $ranges += [pscustomobject]@{ From = $start; To = $previous }
            $start = $null
        }
        if ($null -ne $line -and $null -eq $start) { $start = $line }
        $previous = $line
    }
    return $ranges
}

# Which suites cover a file
$coveredBy = @{}
foreach ($test in $suiteFiles.Keys) {
    foreach ($file in $suiteFiles[$test]) {
        if (-not $coveredBy.ContainsKey($file)) { $coveredBy[$file] = @() }
        $coveredBy[$file] += ($test -replace '^test_', '')
    }
}

$md = [System.Collections.Generic.List[string]]::new()
$md.Add("# Test coverage")
$md.Add("")
$md.Add("![Coverage](docs/coverage-badge.svg)")
$md.Add("")
$cases = ($suiteCases.Values | ForEach-Object { @($_).Count } | Measure-Object -Sum).Sum
$md.Add("**$percent%**${dot}$coveredTotal / $total lines${dot}$($tests.Count) test suites${dot}$cases test cases${dot}" +
        "$(Get-Date -Format 'yyyy-MM-dd')${dot}$measuredAt")
$md.Add("")
$md.Add("## Files")
$md.Add("")
$md.Add("| File | Coverage | Lines | Covered by |")
$md.Add("|---|---:|---:|---|")
foreach ($file in ($files | Sort-Object Coverage, File)) {
    $suites = @($coveredBy[$file.File] | Sort-Object)
    $suiteText = if ($suites.Count -eq 0) { "*none*" }
                 elseif ($suites.Count -le 4) { $suites -join ", " }
                 else { ($suites[0..2] -join ", ") + " and $($suites.Count - 3) more" }
    $md.Add("| [$($file.File)]($($file.File)) | $($file.Coverage)% | $($file.Covered) / $($file.Lines) | $suiteText |")
}
$md.Add("")
$md.Add("## Not covered yet")
$md.Add("")
foreach ($file in ($files | Where-Object { $_.Covered -lt $_.Lines } | Sort-Object Coverage, File)) {
    $ranges = @(Get-UncoveredRanges $lineHits[$file.File])
    $links = foreach ($range in $ranges) {
        if ($range.From -eq $range.To) { "[$($range.From)]($($file.File)#L$($range.From))" }
        else { "[$($range.From)$([char]0x2013)$($range.To)]($($file.File)#L$($range.From)-L$($range.To))" }
    }
    $md.Add("- **$($file.File)** ($($file.Lines - $file.Covered) lines): $($links -join ', ')")
}
$md.Add("")
$md.Add("## Test suites")
$md.Add("")
# "deletingAsksFirst" -> "Deleting asks first"
function ConvertTo-Sentence($name) {
    $words = ($name -creplace '_', ' ' -creplace '([a-z0-9])([A-Z])', '$1 $2' -creplace '([A-Z])([A-Z][a-z])', '$1 $2').Trim().ToLower() -replace '\s+', ' '
    return $words.Substring(0, 1).ToUpper() + $words.Substring(1)
}

foreach ($test in ($tests | Sort-Object)) {
    $names = @($suiteCases[$test])
    $md.Add("<details><summary><b>$test</b>$dot$($names.Count) test cases$dot$(@($suiteFiles[$test]).Count) files</summary>")
    $md.Add("")
    $md.Add("[tests/$test.cpp](tests/$test.cpp)")
    $md.Add("")
    foreach ($name in $names) {
        $md.Add("- $(ConvertTo-Sentence $name)")
    }
    $md.Add("")
    $md.Add("</details>")
    $md.Add("")
}
$markdownPath = Join-Path $repo "COVERAGE.md"
[System.IO.File]::WriteAllText($markdownPath, ($md -join "`n") + "`n", [System.Text.UTF8Encoding]::new($false))

# A small SVG badge, so the README shows the number without asking an outside service
$color = if ($percent -ge 90) { "#2e9e5b" } elseif ($percent -ge 75) { "#a3a31f" } elseif ($percent -ge 50) { "#d08a1d" } else { "#c9372c" }
$value = "$percent%"
$badge = @"
<svg xmlns="http://www.w3.org/2000/svg" width="104" height="20" role="img" aria-label="coverage: $value">
  <title>coverage: $value</title>
  <clipPath id="r"><rect width="104" height="20" rx="3" fill="#fff"/></clipPath>
  <g clip-path="url(#r)">
    <rect width="61" height="20" fill="#555"/>
    <rect x="61" width="43" height="20" fill="$color"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">
    <text x="30.5" y="14">coverage</text>
    <text x="82.5" y="14">$value</text>
  </g>
</svg>
"@
New-Item -ItemType Directory -Force (Join-Path $repo "docs") | Out-Null
[System.IO.File]::WriteAllText((Join-Path $repo "docs\coverage-badge.svg"), $badge, [System.Text.UTF8Encoding]::new($false))
Write-Host "Wrote COVERAGE.md and docs/coverage-badge.svg"
