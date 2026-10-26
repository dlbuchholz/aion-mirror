# Every tests/test_*.cpp is its own test executable. CONFIGURE_DEPENDS re-runs CMake when a
# test file is added or removed, so new tests don't need to be listed anywhere.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(test_source IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source}
        test_db_fixture.h
    )
    target_include_directories(${test_name} PRIVATE ../src)
    target_link_libraries(${test_name}
        PRIVATE
            AionLib
            miniz                                    # test_spreadsheet writes sample spreadsheets
            Qt6::Core
            Qt6::Widgets
            Qt6::Sql
            Qt6::Test
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
