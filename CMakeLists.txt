cmake_minimum_required(VERSION 3.19)
project(Liora VERSION 0.2.0 LANGUAGES C CXX)          # C for thirdparty/miniz

include(CTest)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Svg Test)
# lupdate and lrelease for the translations; without them Liora is built in English only
find_package(Qt6 QUIET OPTIONAL_COMPONENTS LinguistTools)

qt_standard_project_setup()

# Define source files for the main application
set(SRC_DIR src)
set(UI_DIR ui)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/${UI_DIR}")
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.h" "${UI_DIR}/*.ui")
# main.cpp belongs to the program only; everything else is compiled once, into LioraLib, which
# the program and the tests link
list(FILTER SRC_FILES EXCLUDE REGEX "/${SRC_DIR}/main\\.cpp$")
include_directories(${SRC_DIR})

# Git commit for the About page. Read when CMake configures, so it can lag behind
# until the next configure (Qt Creator re-runs CMake when CMakeLists.txt changes).
set(APP_GIT_COMMIT "unknown")
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
        set(APP_GIT_COMMIT "${GIT_COMMIT_OUTPUT}")
    endif()
endif()

# Version header (APP_VERSION etc.) generated from the project version above.
# Only files that include version.h are rebuilt when the version changes.
configure_file(${SRC_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)
configure_file(${SRC_DIR}/appname.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/appname.h @ONLY)   # APP_NAME, the first word of project() above

# Compiler warnings for Liora's own code (not Qt's headers). LIORA_WARNINGS_AS_ERRORS turns them
# into errors, for CI or before a release.
option(LIORA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
add_library(liora_warnings INTERFACE)
if(MSVC)
    target_compile_options(liora_warnings INTERFACE /W4 $<$<BOOL:${LIORA_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(liora_warnings INTERFACE -Wall -Wextra -Wpedantic $<$<BOOL:${LIORA_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

# LIORA_COVERAGE builds with GCC's coverage counters, for scripts/coverage.sh (GCC or Clang only)
option(LIORA_COVERAGE "Build with coverage counters (GCC/Clang)" OFF)
if(LIORA_COVERAGE)
    if(MSVC)
        message(FATAL_ERROR "LIORA_COVERAGE needs GCC or Clang")
    endif()
    target_compile_options(liora_warnings INTERFACE --coverage -O0 -g)
    target_link_options(liora_warnings INTERFACE --coverage)
endif()

# LIORA_SANITIZERS builds with AddressSanitizer and UndefinedBehaviorSanitizer, for the nightly
# check (GCC or Clang only): a memory error or undefined behaviour stops the program with a report
option(LIORA_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)
if(LIORA_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "LIORA_SANITIZERS needs GCC or Clang")
    endif()
    target_compile_options(liora_warnings INTERFACE
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g)
    target_link_options(liora_warnings INTERFACE -fsanitize=address,undefined)
endif()

# Visual Studio builds compile one file after another unless told otherwise (/MP). Ninja already
# runs one compiler per file in parallel.
if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options(/MP)
endif()

# MSVC's incremental linking reuses the .ilk files of earlier links and has produced test
# programs that crash on start until relinked from scratch. Full links are quick here.
if(MSVC)
    foreach(flags CMAKE_EXE_LINKER_FLAGS_DEBUG CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/INCREMENTAL(:YES)?( |$)" "/INCREMENTAL:NO\\2" ${flags} "${${flags}}")
    endforeach()
endif()

# miniz reads the zip archives spreadsheets are (thirdparty/miniz/README.md). Its own target, so
# Liora's warnings don't apply to it; zlib-style names off, since Qt brings zlib.
add_library(miniz STATIC thirdparty/miniz/miniz.c)
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniz)
target_compile_definitions(miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES MINIZ_NO_STDIO MINIZ_NO_TIME)

add_library(LioraLib STATIC ${SRC_FILES})
target_link_libraries(LioraLib PUBLIC liora_warnings)   # The app and the tests get the same warnings
target_link_libraries(LioraLib PRIVATE miniz)
# qDebug is for working on Liora. Warnings stay on in every build.
target_compile_definitions(LioraLib PUBLIC $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>)
target_include_directories(LioraLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(LioraLib PUBLIC
    Qt::Core
    Qt::Widgets
    Qt::Sql
    Qt::Svg
)

# Bundle the monochrome UI icons into the library as Qt resources (:/icons/<name>.svg).
# CONFIGURE_DEPENDS picks up new icons.
file(GLOB ICON_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg")
# The app's logo: the window and taskbar icon, the .exe icon, the Linux icon, and in the app itself.
# Always :/liora-logo.svg and generated/liora.svg, whichever file it is, so changing it is only here.
set(APP_LOGO assets/liora-logo-dark.svg)
set_source_files_properties(${APP_LOGO} PROPERTIES QT_RESOURCE_ALIAS liora-logo.svg)
list(APPEND ICON_FILES ${APP_LOGO})                       # :/liora-logo.svg
configure_file(${APP_LOGO} ${CMAKE_CURRENT_BINARY_DIR}/generated/liora.svg COPYONLY)   # For the AppImage
qt_add_resources(LioraLib "icons"
    PREFIX "/"
    BASE "assets"
    FILES ${ICON_FILES}
)

# The database migrations too (:/migrations/<file>.sql), so Liora needs no files next to the
# program. CONFIGURE_DEPENDS picks up new migration files.
file(GLOB MIGRATION_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/db/migrations/*.sql")
qt_add_resources(LioraLib "migrations"
    PREFIX "/"
    BASE "db"
    FILES ${MIGRATION_FILES}
)

# The translations (translations/liora_<code>.ts, see CONTRIBUTING.md), built into the library as
# :/i18n/liora_<code>.qm, with Qt's own texts for those languages (:/i18n/qtbase_<code>.qm) when
# this Qt has them. "cmake --build <dir> --target update_translations" adds new and changed texts
# from the code to the .ts files. APP_HAS_TRANSLATIONS tells the tests the translations are in.
set(LIORA_LANGUAGES de)

# How complete each translation is (translationprogress.h), for the language list in Settings.
# Counted again when a .ts file changes.
include(cmake/TranslationProgress.cmake)
set(LIORA_TRANSLATION_PROGRESS "")
foreach(language IN LISTS LIORA_LANGUAGES)
    set(ts_file ${CMAKE_CURRENT_SOURCE_DIR}/translations/liora_${language}.ts)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ts_file})
    liora_translation_progress(${ts_file} translated total)
    string(APPEND LIORA_TRANSLATION_PROGRESS "    {\"${language}\", ${translated}, ${total}},\n")
endforeach()
configure_file(${SRC_DIR}/translationprogress.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/translationprogress.h @ONLY)

if(TARGET Qt6::lrelease)
    set(LIORA_TS_FILES "")
    set(QT_TRANSLATION_FILES "")
    foreach(language IN LISTS LIORA_LANGUAGES)
        list(APPEND LIORA_TS_FILES translations/liora_${language}.ts)
        set(qt_qm "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}/qtbase_${language}.qm")
        if(EXISTS "${qt_qm}")
            list(APPEND QT_TRANSLATION_FILES "${qt_qm}")
        endif()
    endforeach()
    qt_add_translations(LioraLib
        TS_FILES ${LIORA_TS_FILES}
        RESOURCE_PREFIX "/i18n"
        LUPDATE_OPTIONS -no-obsolete -locations none
    )
    if(QT_TRANSLATION_FILES)
        qt_add_resources(LioraLib "qt_translations"
            PREFIX "/i18n"
            BASE "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}"
            FILES ${QT_TRANSLATION_FILES}
        )
    endif()
    target_compile_definitions(LioraLib PUBLIC APP_HAS_TRANSLATIONS)
else()
    message(WARNING "Qt's LinguistTools weren't found: Liora is built in English only")
endif()

qt_add_executable(Liora
    WIN32 MACOSX_BUNDLE
    ${SRC_DIR}/main.cpp
)
# The .exe icon in Win Explorer, made from the logo during the build (tools/makeico), so it always
# matches the logo (APP_LOGO). The helper runs in Qt's bin folder, so Windows finds Qt's DLLs.
# When cross-compiling (CI builds Windows on Linux) the helper is a Windows program too, so it
# runs through CMAKE_CROSSCOMPILING_EMULATOR (Wine).
if(WIN32)
    add_executable(liora_makeico tools/makeico/makeico.cpp)
    target_link_libraries(liora_makeico PRIVATE Qt::Gui Qt::Svg)
    set(LIORA_ICON ${CMAKE_CURRENT_BINARY_DIR}/generated/liora.ico)
    set(LIORA_LOGO ${CMAKE_CURRENT_SOURCE_DIR}/${APP_LOGO})
    set(LIORA_MAKEICO_LAUNCHER "")
    if(CMAKE_CROSSCOMPILING)
        set(LIORA_MAKEICO_LAUNCHER ${CMAKE_CROSSCOMPILING_EMULATOR})
    endif()
    add_custom_command(
        OUTPUT ${LIORA_ICON}
        COMMAND ${LIORA_MAKEICO_LAUNCHER} $<TARGET_FILE:liora_makeico> ${LIORA_LOGO} ${LIORA_ICON}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:Qt::Core>
        DEPENDS liora_makeico ${LIORA_LOGO}
        COMMENT "Making liora.ico from the logo"
        VERBATIM)
    set(LIORA_RC ${CMAKE_CURRENT_BINARY_DIR}/generated/liora.rc)
    file(WRITE ${LIORA_RC} "IDI_ICON1 ICON \"${LIORA_ICON}\"\n")
    set_source_files_properties(${LIORA_RC} PROPERTIES OBJECT_DEPENDS ${LIORA_ICON})   # Recompile when the icon changes
    target_sources(Liora PRIVATE ${LIORA_RC} ${LIORA_ICON})
endif()

target_link_libraries(Liora PRIVATE LioraLib)

include(GNUInstallDirs)

install(TARGETS Liora
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Packages for a distribution use its Qt, so installing mustn't copy Qt along
option(LIORA_USE_SYSTEM_QT "Don't install Qt with Liora, for a distribution's package" OFF)
if(NOT LIORA_USE_SYSTEM_QT)
    qt_generate_deploy_app_script(
        TARGET Liora
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/liora.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${APP_LOGO} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME liora.svg)
    set(LIORA_DOC_DIR ${CMAKE_INSTALL_DATADIR}/doc/liora)
    install(FILES LICENSE packaging/linux/THIRD-PARTY DESTINATION ${LIORA_DOC_DIR})
    install(FILES assets/icons/LICENSE DESTINATION ${LIORA_DOC_DIR} RENAME LICENSE-icons)
    install(FILES thirdparty/miniz/LICENSE DESTINATION ${LIORA_DOC_DIR} RENAME LICENSE-miniz)
endif()

# The RPM and the DEB (scripts/package-rpm.sh, scripts/package-deb.sh)
set(CPACK_PACKAGE_NAME liora)
set(CPACK_PACKAGE_VENDOR "The Liora contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Personal finance")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)
set(CPACK_RPM_PACKAGE_LICENSE GPL-3.0-or-later)
set(CPACK_RPM_PACKAGE_URL https://codeberg.org/spectre_0/aion)
set(CPACK_RPM_PACKAGE_DESCRIPTION "A personal finance app for the desktop.")
# Qt loads these as plugins at runtime, so rpmbuild can't find them. The workflow checks the names.
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtsvg")
# These folders belong to other packages
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications /usr/share/icons /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable /usr/share/icons/hicolor/scalable/apps)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "The Liora contributors")
set(CPACK_DEBIAN_PACKAGE_SECTION misc)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE https://codeberg.org/spectre_0/aion)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Personal finance\n A personal finance app for the desktop.")
# Finds the libraries Liora links. Qt's plugins are loaded at runtime, so they're named here, and the
# workflow checks the names.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite, qt6-svg-plugins")
include(CPack)

add_subdirectory(tests)
