# Writes a badge per translation (docs/translation-<code>-badge.svg) with how much of it is done,
# and lists them in README.md between its "translations" markers.
#
#   cmake -P scripts/translation-status.cmake
#
# Counts like the build does (cmake/TranslationProgress.cmake). The languages and their names come
# from Language::available() in src/language.cpp. CI runs this when a translation changes on
# develop and commits the result (.forgejo/workflows/translations.yml).
cmake_minimum_required(VERSION 3.21)
get_filename_component(repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include(${repo}/cmake/TranslationProgress.cmake)

# {"de", "Deutsch"} in Language::available()
file(READ ${repo}/src/language.cpp language_source)
string(REGEX MATCH "available\\(\\) {[^}]*return {([^;]*)};" available "${language_source}")
string(REGEX MATCHALL "{\"[a-z]+\", \"[^\"]+\"}" options "${CMAKE_MATCH_1}")
if(NOT options)
    message(FATAL_ERROR "No languages found in Language::available() (src/language.cpp)")
endif()

# A badge about as wide as its text: Verdana at 11px is about 7px a letter
function(write_badge path label value color)
    string(LENGTH "${label}" label_length)
    string(LENGTH "${value}" value_length)
    math(EXPR label_width "${label_length} * 7 + 12")
    math(EXPR value_width "${value_length} * 7 + 12")
    math(EXPR width "${label_width} + ${value_width}")
    math(EXPR label_middle "${label_width} * 5")            # In tenths, for the text's centre
    math(EXPR value_middle "${label_width} * 10 + ${value_width} * 5")
    file(WRITE "${path}" "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"${width}\" height=\"20\" role=\"img\" aria-label=\"${label}: ${value}\">
  <title>${label}: ${value}</title>
  <clipPath id=\"r\"><rect width=\"${width}\" height=\"20\" rx=\"3\" fill=\"#fff\"/></clipPath>
  <g clip-path=\"url(#r)\">
    <rect width=\"${label_width}\" height=\"20\" fill=\"#555\"/>
    <rect x=\"${label_width}\" width=\"${value_width}\" height=\"20\" fill=\"${color}\"/>
  </g>
  <g fill=\"#fff\" text-anchor=\"middle\" font-family=\"Verdana,Geneva,DejaVu Sans,sans-serif\" font-size=\"11\">
    <text x=\"${label_middle}\" y=\"140\" transform=\"scale(.1)\" font-size=\"110\">${label}</text>
    <text x=\"${value_middle}\" y=\"140\" transform=\"scale(.1)\" font-size=\"110\">${value}</text>
  </g>
</svg>
")
endfunction()

set(lines "")
foreach(option IN LISTS options)
    string(REGEX MATCH "{\"([a-z]+)\", \"([^\"]+)\"}" _ "${option}")
    set(code ${CMAKE_MATCH_1})
    set(name ${CMAKE_MATCH_2})
    if(code STREQUAL "en")
        continue()                                       # Written in the code
    endif()
    set(ts_file ${repo}/translations/aion_${code}.ts)
    if(NOT EXISTS ${ts_file})
        message(FATAL_ERROR "${name} has no translations/aion_${code}.ts")
    endif()
    aion_translation_progress(${ts_file} translated total)
    aion_translation_percent(${translated} ${total} percent)
    if(percent GREATER_EQUAL 100)
        set(color "#2e9e5b")
    elseif(percent GREATER_EQUAL 75)
        set(color "#a3a31f")
    elseif(percent GREATER_EQUAL 50)
        set(color "#d08a1d")
    else()
        set(color "#c9372c")
    endif()
    write_badge(${repo}/docs/translation-${code}-badge.svg "${name}" "${percent}%" "${color}")
    string(APPEND lines "[![${name}: ${percent}% translated](docs/translation-${code}-badge.svg)](translations/aion_${code}.ts)\n")
    message(STATUS "${name}: ${translated} of ${total} texts, ${percent}%")
endforeach()

# Between the markers in the README, everything else stays
set(begin "<!-- translations: written by scripts/translation-status.cmake -->")
set(end "<!-- /translations -->")
file(READ ${repo}/README.md readme)
string(FIND "${readme}" "${begin}" begin_at)
string(FIND "${readme}" "${end}" end_at)
if(begin_at LESS 0 OR end_at LESS begin_at)
    message(FATAL_ERROR "README.md lacks the markers ${begin} and ${end}")
endif()
string(SUBSTRING "${readme}" 0 ${begin_at} before)
string(SUBSTRING "${readme}" ${end_at} -1 after)
file(WRITE ${repo}/README.md "${before}${begin}\n${lines}${after}")
