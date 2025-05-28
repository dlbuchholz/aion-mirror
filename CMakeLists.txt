cmake_minimum_required(VERSION 3.19)
project(Aion LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Test)

qt_standard_project_setup()

# Define source files for the main application
set(SRC_DIR src)
set(UI_DIR ui)
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.h" "${UI_DIR}/*.ui")
include_directories(${SRC_DIR})

# Define paths
set(SQL_MIGRATIONS_DIR "${CMAKE_SOURCE_DIR}/db/migrations")
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")

add_library(AionLib STATIC ${SRC_FILES})
target_include_directories(AionLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(AionLib PUBLIC SQL_MIGRATIONS_DIR="${SQL_MIGRATIONS_DIR}")
target_compile_definitions(AionLib PUBLIC ASSETS_DIR="${ASSETS_DIR}")
target_link_libraries(AionLib
    Qt::Core
    Qt::Widgets
    Qt::Sql
)

qt_add_executable(Aion
    WIN32 MACOSX_BUNDLE
    ${SRC_FILES}
)

target_link_libraries(Aion PRIVATE AionLib)

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

add_subdirectory(tests)
