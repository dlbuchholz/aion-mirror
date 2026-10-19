cmake_minimum_required(VERSION 3.19)
project(Aion VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Svg Test)

qt_standard_project_setup()

# Define source files for the main application
set(SRC_DIR src)
set(UI_DIR ui)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/${UI_DIR}")
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.h" "${UI_DIR}/*.ui")
include_directories(${SRC_DIR})

# Define paths
set(SQL_MIGRATIONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/db/migrations")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

# Git commit for the About page. Read when CMake configures, so it can lag behind
# until the next configure (Qt Creator re-runs CMake when CMakeLists.txt changes).
set(AION_GIT_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_COMMIT_RESULT)
    if(GIT_COMMIT_RESULT EQUAL 0 AND GIT_COMMIT_OUTPUT)
        set(AION_GIT_COMMIT "${GIT_COMMIT_OUTPUT}")
    endif()
endif()

# Version header (AION_VERSION etc.) generated from the project version above.
# Only files that include version.h are rebuilt when the version changes.
configure_file(${SRC_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)

# Compiler warnings for Aion's own code (not Qt's headers). AION_WARNINGS_AS_ERRORS turns them
# into errors, for CI or before a release.
option(AION_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
add_library(aion_warnings INTERFACE)
if(MSVC)
    target_compile_options(aion_warnings INTERFACE /W4 $<$<BOOL:${AION_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(aion_warnings INTERFACE -Wall -Wextra -Wpedantic $<$<BOOL:${AION_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

# MSVC's incremental linking reuses the .ilk files of earlier links and has produced test
# programs that crash on start until relinked from scratch. Full links are quick here.
if(MSVC)
    foreach(flags CMAKE_EXE_LINKER_FLAGS_DEBUG CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/INCREMENTAL(:YES)?( |$)" "/INCREMENTAL:NO\\2" ${flags} "${${flags}}")
    endforeach()
endif()

add_library(AionLib STATIC ${SRC_FILES})
target_link_libraries(AionLib PUBLIC aion_warnings)   # The app and the tests get the same warnings
target_include_directories(AionLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(AionLib PUBLIC SQL_MIGRATIONS_DIR="${SQL_MIGRATIONS_DIR}")
target_compile_definitions(AionLib PUBLIC ASSETS_DIR="${ASSETS_DIR}")
target_link_libraries(AionLib PUBLIC
    Qt::Core
    Qt::Widgets
    Qt::Sql
    Qt::Svg
)

# Bundle the monochrome UI icons into the library as Qt resources (:/icons/<name>.svg)
file(GLOB ICON_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg")
qt_add_resources(AionLib "icons"
    PREFIX "/"
    BASE "assets"
    FILES ${ICON_FILES}
)

qt_add_executable(Aion
    WIN32 MACOSX_BUNDLE
    ${SRC_FILES}
)

target_link_libraries(Aion PRIVATE AionLib)

# Print the path for debugging purposes
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
