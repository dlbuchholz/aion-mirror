# Every tests/test_*.cpp is its own test executable. CONFIGURE_DEPENDS re-runs CMake when a
# test file is added or removed, so new tests don't need to be listed anywhere.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(test_source IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source}
        isolated_settings.cpp                    # Never the real app's settings
        test_db_fixture.h
        test_spreadsheet_files.h
        fake_translator.h
    )
    target_include_directories(${test_name} PRIVATE ../src)
    # Where the translation sources are, to check that every text has its translation
    target_compile_definitions(${test_name} PRIVATE APP_TRANSLATIONS_DIR="${PROJECT_SOURCE_DIR}/translations")
    target_link_libraries(${test_name}
        PRIVATE
            LioraLib
            miniz                                    # test_spreadsheet_files.h writes sample spreadsheets
            Qt6::Core
            Qt6::Widgets
            Qt6::Sql
            Qt6::Test
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# How the build counts a translation's progress (cmake/TranslationProgress.cmake)
add_test(NAME translation_progress COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/translation_progress_test.cmake)
