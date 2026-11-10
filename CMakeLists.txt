cmake_minimum_required(VERSION 3.25)
project(calculate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets LinguistTools)
qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES es_ES)

include(FetchContent)
FetchContent_Declare(calculate-core
    GIT_REPOSITORY https://github.com/bentxr/calculate-core.git
    GIT_TAG        release-staging
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(calculate-core)
if(EMSCRIPTEN)  # the multithread Qt kit links a shared-memory module: every object needs -pthread
    target_compile_options(calculate-core PRIVATE -pthread)
endif()

qt_add_library(calculate-ui STATIC
    src/constanttext.cpp
    src/detailscard.cpp
    src/devicekeyboard.cpp
    src/entry.cpp
    src/formulatip.cpp
    src/functiontext.cpp
    src/icons.cpp
    src/inspector.cpp
    src/keybutton.cpp
    src/keypad.cpp
    src/keysizing.cpp
    src/lcd.cpp
    src/licences.cpp
    src/longpress.cpp
    src/mainwindow.cpp
    src/popupplacement.cpp
    src/presenter.cpp
    src/settings.cpp
    src/settingsfile.cpp
    src/typechooser.cpp
    src/typeset.cpp
    src/typing.cpp
    src/undostack.cpp
    src/worker.cpp
)
if(EMSCRIPTEN)  # the browser's asynchronous clipboard
    target_sources(calculate-ui PRIVATE src/webclipboard.cpp)
endif()
target_include_directories(calculate-ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(calculate-ui PUBLIC Qt6::Widgets calculate-core)
target_compile_options(calculate-ui PRIVATE -Wall -Wextra -Wpedantic -Werror)

# The screen's typeface: JetBrains Mono, SIL Open Font License 1.1 (its licence ships with it).
FetchContent_Declare(jetbrainsmono
    URL      https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip
    URL_HASH SHA256=6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(jetbrainsmono)
set(font_files ${jetbrainsmono_SOURCE_DIR}/fonts/ttf/JetBrainsMono-Regular.ttf)
set_source_files_properties(${jetbrainsmono_SOURCE_DIR}/fonts/ttf/JetBrainsMono-Regular.ttf PROPERTIES QT_RESOURCE_ALIAS JetBrainsMono-Regular.ttf)
qt_add_resources(calculate-ui fonts PREFIX /fonts FILES ${font_files})
# The program's notices: NOTICE and the licence files of every component it ships, as licensing/manifest.json lists
# them (Qt under the LGPL-3.0, the third-party code inside Qt, the typeface, Boost; test-only components left out).
file(READ ${PROJECT_SOURCE_DIR}/licensing/manifest.json manifest)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/licensing/manifest.json)
set(shipped_licences ${PROJECT_SOURCE_DIR}/NOTICE)
string(JSON components LENGTH "${manifest}" components)
math(EXPR last "${components} - 1")
foreach(i RANGE ${last})
    string(JSON use GET "${manifest}" components ${i} use)
    if(use STREQUAL "runtime")
        string(JSON files LENGTH "${manifest}" components ${i} licenceFiles)
        math(EXPR last_file "${files} - 1")
        foreach(j RANGE ${last_file})
            string(JSON path GET "${manifest}" components ${i} licenceFiles ${j} path)
            list(APPEND shipped_licences ${PROJECT_SOURCE_DIR}/${path})
        endforeach()
    endif()
endforeach()
list(REMOVE_DUPLICATES shipped_licences)
qt_add_resources(calculate-ui licences PREFIX /licences BASE ${PROJECT_SOURCE_DIR} FILES ${shipped_licences})
# Qt's own texts in Spanish (a text field's menu, the standard buttons): Qt's translation, beside the program's
# (where Qt's package says, or where Qt's own qtpaths says, for a Qt found through a linked folder)
set(qt_translations ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS})
if(TARGET Qt6::qtpaths)
    get_target_property(qtpaths Qt6::qtpaths IMPORTED_LOCATION)
    execute_process(COMMAND ${qtpaths} --query QT_INSTALL_TRANSLATIONS OUTPUT_VARIABLE host_translations
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    list(APPEND qt_translations ${host_translations})
endif()
find_file(qt_spanish qtbase_es.qm PATHS ${qt_translations} NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT qt_spanish)
    message(FATAL_ERROR "Qt's Spanish translation (qtbase_es.qm) is not in ${qt_translations}")
endif()
set_source_files_properties(${qt_spanish} PROPERTIES QT_RESOURCE_ALIAS qtbase_es.qm)
qt_add_resources(calculate-ui qttranslations PREFIX /i18n FILES ${qt_spanish})
qt_add_translations(TARGETS calculate-ui TS_FILE_DIR i18n NO_GENERATE_PLURALS_TS_FILE
    LUPDATE_OPTIONS -no-obsolete)

qt_add_executable(calculate src/main.cpp)
target_link_libraries(calculate PRIVATE calculate-ui)
set_target_properties(calculate PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()
