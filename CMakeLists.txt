cmake_minimum_required(VERSION 3.19)
project(Aion LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql)

qt_standard_project_setup()

# Define source files for the main application
set(SRC_DIR src)
set(UI_DIR ui)
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.h" "${UI_DIR}/*.ui")

include_directories(${SRC_DIR})

qt_add_executable(Aion
    WIN32 MACOSX_BUNDLE
    ${SRC_FILES}
)

target_link_libraries(Aion
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Sql
)

# Define the SQL migrations directory path
set(SQL_MIGRATIONS_DIR "${CMAKE_SOURCE_DIR}/db/migrations")
target_compile_definitions(Aion PRIVATE SQL_MIGRATIONS_DIR="${SQL_MIGRATIONS_DIR}")
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
target_compile_definitions(Aion PRIVATE ASSETS_DIR="${ASSETS_DIR}")

# Print the path for debugging purposes (optional)
message(STATUS "SQL Migrations Directory: ${SQL_MIGRATIONS_DIR}")
install(DIRECTORY ${SQL_MIGRATIONS_DIR} DESTINATION migrations)

include(GNUInstallDirs)

install(TARGETS Aion
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Aion
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
