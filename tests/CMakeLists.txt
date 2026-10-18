set(TEST_TARGETS
    test_account
    test_integration
    test_migrationmanager
    test_csvparser
    test_csvimport
    test_money
    test_transactionlistmodel
    test_transactionspage
    test_preferenceswidget
    test_settingspage
    test_sidebarwidget
    test_databasesession
    test_startwindow
    test_systeminfo
    test_budget
    test_user_category
    test_settings
)

foreach(test_name IN LISTS TEST_TARGETS)
    add_executable(${test_name} ${test_name}.cpp
        test_db_fixture.h
    )
    target_include_directories(${test_name} PRIVATE ../src)
    target_link_libraries(${test_name}
        PRIVATE
                        AionLib
            Qt6::Core
            Qt6::Widgets
            Qt6::Sql
            Qt6::Test
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
