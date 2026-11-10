# How much of a translation is done, counted from its .ts file. Used by the build (the language
# list in Settings) and by scripts/translation-status.cmake (the badges in the README), so both
# count the same way.
#
#   aion_translation_progress(<ts file> <translated var> <total var>)
#
# Every text the code has counts towards the total; texts no longer in the code (vanished or
# obsolete) don't. A text counts as translated when its translation isn't empty and isn't marked
# unfinished.
function(aion_translation_progress ts_file translated_var total_var)
    file(READ "${ts_file}" content)
    string(REGEX MATCHALL "<translation[ >]" all "${content}")
    string(REGEX MATCHALL "<translation type=\"(vanished|obsolete)\"" gone "${content}")
    string(REGEX MATCHALL "<translation>[^<]" done "${content}")
    list(LENGTH all all_count)
    list(LENGTH gone gone_count)
    list(LENGTH done done_count)
    math(EXPR total "${all_count} - ${gone_count}")
    set(${translated_var} ${done_count} PARENT_SCOPE)
    set(${total_var} ${total} PARENT_SCOPE)
endfunction()

# The share in whole percent, rounded down so that only a complete translation shows 100
#   aion_translation_percent(<translated> <total> <percent var>)
function(aion_translation_percent translated total percent_var)
    if(total GREATER 0)
        math(EXPR percent "${translated} * 100 / ${total}")
    else()
        set(percent 0)
    endif()
    set(${percent_var} ${percent} PARENT_SCOPE)
endfunction()
