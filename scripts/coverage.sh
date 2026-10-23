#!/usr/bin/env bash
# Measures test coverage of src/ with GCC's coverage counters and gcovr.
#
#   scripts/coverage.sh <build-dir> [--publish]
#
# The build folder must be configured with -DAION_COVERAGE=ON (GCC or Clang) and built, e.g.:
#   cmake -S . -B build-coverage -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAION_COVERAGE=ON
#   cmake --build build-coverage
#
# Runs every test registered with CTest on its own, so the report can say which suites run
# which file, then merges the results and writes:
#   <build-dir>/coverage/html/index.html   browsable report
# and prints line coverage per file. With --publish it also writes COVERAGE.md (per file:
# coverage, the test suites that run it, links to the uncovered lines, and each suite's test
# cases) and docs/coverage-badge.svg, which the README shows.
#
# Needs gcovr (Fedora: dnf install gcovr, elsewhere: pip install gcovr) and Python 3, which
# gcovr runs on anyway and which writes the report below.
set -euo pipefail

build_dir=$(realpath "$1")
publish=false
[[ ${2:-} == --publish ]] && publish=true
repo=$(realpath "$(dirname "$0")/..")
output="$build_dir/coverage"
jobs=$(nproc 2>/dev/null || echo 2)                  # gcovr reads the object files in parallel

command -v gcovr > /dev/null || { echo "gcovr not found (dnf install gcovr, or pip install gcovr)" >&2; exit 1; }
grep -q '^AION_COVERAGE:BOOL=ON' "$build_dir/CMakeCache.txt" 2>/dev/null \
    || { echo "$build_dir isn't configured with -DAION_COVERAGE=ON" >&2; exit 1; }

# Only the tests CTest knows about (old test programs may still lie around)
mapfile -t tests < <(ctest --test-dir "$build_dir" -N | sed -n 's/^ *Test *#[0-9]*: *//p')
((${#tests[@]})) || { echo "No tests found in $build_dir; build it first" >&2; exit 1; }

rm -rf "$output"
mkdir -p "$output/suites" "$output/html"
failed=()
for test in "${tests[@]}"; do
    echo "Running $test"
    # Counters from earlier runs would count for this suite too
    find "$build_dir" -name '*.gcda' -delete
    ctest --test-dir "$build_dir" -R "^$test\$" > "$output/suites/$test.log" 2>&1 || failed+=("$test")
    gcovr -j "$jobs" --root "$repo" --filter "$repo/src/" --json "$output/suites/$test.json" "$build_dir" > /dev/null
    # QtTest lists a program's test functions with -functions
    "$build_dir/$test" -functions > "$output/suites/$test.functions" 2>/dev/null || true
done

tracefiles=()
for json in "$output"/suites/*.json; do tracefiles+=(--add-tracefile "$json"); done
gcovr --root "$repo" "${tracefiles[@]}" --html-details "$output/html/index.html" > /dev/null

commit=$(git -C "$repo" rev-parse HEAD 2>/dev/null || true)
dirty=$(git -C "$repo" status --porcelain --untracked-files=no 2>/dev/null || true)

# python3 on Linux; stupidly, on Windows that name can be a stub pointing to the Microsoft Store 😒
python=python3
python3 -c '' 2>/dev/null || python=python
"$python" - "$repo" "$output/suites" "$publish" "$commit" "${dirty:+modified}" <<'PYTHON'
import datetime, json, math, pathlib, re, sys

repo, suites_dir, publish, commit, dirty = sys.argv[1:6]
repo, suites_dir = pathlib.Path(repo), pathlib.Path(suites_dir)
tests = sorted(p.stem for p in suites_dir.glob("*.json"))

def lines_of(report):
    """file -> {line number -> hit} for the code lines in a gcovr JSON report"""
    files = {}
    for entry in report["files"]:
        lines = files.setdefault(entry["file"].replace("\\", "/"), {})
        for line in entry["lines"]:
            if line.get("gcovr/noncode"):
                continue
            number = line["line_number"]
            lines[number] = lines.get(number, False) or line["count"] > 0
    return files

# A line counts if any suite ran it. Every test program runs each file's global constants at
# startup, so a suite only counts for a file once it runs a real part of it: 5 lines, or a
# quarter of a small file.
merged, suite_files, suite_cases = {}, {}, {}
for test in tests:
    files = lines_of(json.loads((suites_dir / f"{test}.json").read_text()))
    suite_files[test] = []
    for file, lines in files.items():
        target = merged.setdefault(file, {})
        for number, hit in lines.items():
            target[number] = target.get(number, False) or hit
        hit = sum(lines.values())
        if hit > 0 and hit >= min(5, math.ceil(len(lines) / 4)):
            suite_files[test].append(file)
    functions = (suites_dir / f"{test}.functions").read_text(errors="replace").split()
    suite_cases[test] = [name[:-2] for name in functions if name.endswith("()")
                         and name not in ("initTestCase()", "cleanupTestCase()", "init()", "cleanup()")]

rows = []
for file, lines in merged.items():
    covered = sum(lines.values())
    rows.append((round(100 * covered / len(lines)) if lines else 100, file, covered, len(lines)))
rows.sort()
total = sum(row[3] for row in rows)
covered_total = sum(row[2] for row in rows)
percent = round(100 * covered_total / total) if total else 100

width = max(len(row[1]) for row in rows)
for coverage, file, covered, count in rows:
    print(f"{file:<{width}}  {coverage:>3}%  {covered:>5} / {count}")
print(f"Total: {covered_total} of {total} lines covered ({percent}%)")

if publish != "true":
    sys.exit(0)

# --- COVERAGE.md and the badge, for the repository ---

def ranges(lines):
    """Uncovered lines as (from, to) ranges"""
    result = []
    for number in sorted(n for n, hit in lines.items() if not hit):
        if result and number == result[-1][1] + 1:
            result[-1][1] = number
        else:
            result.append([number, number])
    return result

def sentence(name):
    # deletingAsksFirst -> Deleting asks first
    words = re.sub(r"([A-Z])([A-Z][a-z])", r"\1 \2", re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name.replace("_", " ")))
    words = re.sub(r"\s+", " ", words).strip().lower()
    return words[:1].upper() + words[1:]

covered_by = {}
for test, files in suite_files.items():
    for file in files:
        covered_by.setdefault(file, []).append(test.removeprefix("test_"))

dot = " · "
measured = f"`{commit[:7]}`" + (" (modified)" if dirty else "") if commit else "unknown commit"
cases = sum(len(names) for names in suite_cases.values())
md = [
    f"<!-- Measured at {commit or 'unknown'} by scripts/coverage.sh -->",
    "# Test coverage", "",
    "![Coverage](docs/coverage-badge.svg)", "",
    f"**{percent}%**{dot}{covered_total} / {total} lines{dot}{len(tests)} test suites{dot}{cases} test cases"
    f"{dot}{datetime.date.today().isoformat()}{dot}{measured}", "",
    "## Files", "",
    "| File | Coverage | Lines | Covered by |",
    "|---|---:|---:|---|",
]
for coverage, file, covered, count in rows:
    suites = sorted(covered_by.get(file, []))
    text = ("*none*" if not suites else ", ".join(suites) if len(suites) <= 4
            else ", ".join(suites[:3]) + f" and {len(suites) - 3} more")
    md.append(f"| [{file}]({file}) | {coverage}% | {covered} / {count} | {text} |")
md += ["", "## Not covered yet", ""]
for coverage, file, covered, count in rows:
    if covered < count:
        links = [f"[{a}]({file}#L{a})" if a == b else f"[{a}–{b}]({file}#L{a}-L{b})" for a, b in ranges(merged[file])]
        md.append(f"- **{file}** ({count - covered} lines): {', '.join(links)}")
md += ["", "## Test suites", ""]
for test in tests:
    names = suite_cases[test]
    md += [f"<details><summary><b>{test}</b>{dot}{len(names)} test cases{dot}{len(suite_files[test])} files</summary>", "",
           f"[tests/{test}.cpp](tests/{test}.cpp)", ""]
    md += [f"- {sentence(name)}" for name in names]
    md += ["", "</details>", ""]
(repo / "COVERAGE.md").write_text("\n".join(md) + "\n", encoding="utf-8")

# A small SVG badge, so the README shows the number without asking an outside service
color = "#2e9e5b" if percent >= 90 else "#a3a31f" if percent >= 75 else "#d08a1d" if percent >= 50 else "#c9372c"
value = f"{percent}%"
badge = f"""<svg xmlns="http://www.w3.org/2000/svg" width="104" height="20" role="img" aria-label="coverage: {value}">
  <title>coverage: {value}</title>
  <clipPath id="r"><rect width="104" height="20" rx="3" fill="#fff"/></clipPath>
  <g clip-path="url(#r)">
    <rect width="61" height="20" fill="#555"/>
    <rect x="61" width="43" height="20" fill="{color}"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">
    <text x="30.5" y="14">coverage</text>
    <text x="82.5" y="14">{value}</text>
  </g>
</svg>
"""
(repo / "docs").mkdir(exist_ok=True)
(repo / "docs" / "coverage-badge.svg").write_text(badge, encoding="utf-8")
print("Wrote COVERAGE.md and docs/coverage-badge.svg")
PYTHON

echo "Report: $output/html/index.html"
if ((${#failed[@]})); then
    echo "These tests failed while measuring: ${failed[*]}" >&2
fi
