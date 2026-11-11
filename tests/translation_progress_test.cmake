# Checks how cmake/TranslationProgress.cmake counts, on tests/data/partial.ts: 4 of its 8 texts
# in the code are translated (the unfinished, the not started and the empty ones aren't; the
# vanished and obsolete ones don't count at all; one with plural forms counts once). Run by
# CTest as "translation_progress".
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/TranslationProgress.cmake)

function(expect what actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: ${actual}, expected ${expected}")
    endif()
endfunction()

aion_translation_progress(${CMAKE_CURRENT_LIST_DIR}/data/partial.ts translated total)
expect("translated" "${translated}" 4)
expect("total" "${total}" 8)

aion_translation_percent(${translated} ${total} percent)
expect("percent" "${percent}" 50)
aion_translation_percent(2 3 percent)
expect("2 of 3, rounded down" "${percent}" 66)
aion_translation_percent(972 973 percent)
expect("all but one" "${percent}" 99)
aion_translation_percent(5 5 percent)
expect("all" "${percent}" 100)
aion_translation_percent(0 0 percent)
expect("nothing to translate" "${percent}" 0)

message(STATUS "translation progress counts as expected")
