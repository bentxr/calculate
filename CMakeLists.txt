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
    src/detailscard.cpp
    src/entry.cpp
    src/formulatip.cpp
    src/keypad.cpp
    src/keysizing.cpp
    src/lcd.cpp
    src/mainwindow.cpp
    src/popupplacement.cpp
    src/presenter.cpp
    src/settings.cpp
    src/typechooser.cpp
    src/typeset.cpp
    src/typing.cpp
    src/undostack.cpp
    src/worker.cpp
)
target_include_directories(calculate-ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(calculate-ui PUBLIC Qt6::Widgets calculate-core)
target_compile_options(calculate-ui PRIVATE -Wall -Wextra -Wpedantic -Werror)

# The screen's typeface: JetBrains Mono, SIL Open Font License 1.1 (its licence ships with it).
FetchContent_Declare(jetbrainsmono
    URL      https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip
    URL_HASH SHA256=6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf
    SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(jetbrainsmono)
set(font_files ${jetbrainsmono_SOURCE_DIR}/fonts/ttf/JetBrainsMono-Regular.ttf ${jetbrainsmono_SOURCE_DIR}/OFL.txt)
set_source_files_properties(${jetbrainsmono_SOURCE_DIR}/fonts/ttf/JetBrainsMono-Regular.ttf PROPERTIES QT_RESOURCE_ALIAS JetBrainsMono-Regular.ttf)
set_source_files_properties(${jetbrainsmono_SOURCE_DIR}/OFL.txt PROPERTIES QT_RESOURCE_ALIAS JetBrainsMono-OFL.txt)
qt_add_resources(calculate-ui fonts PREFIX /fonts FILES ${font_files})
qt_add_translations(TARGETS calculate-ui TS_FILE_DIR i18n NO_GENERATE_PLURALS_TS_FILE
    LUPDATE_OPTIONS -no-obsolete)

qt_add_executable(calculate src/main.cpp)
target_link_libraries(calculate PRIVATE calculate-ui)
set_target_properties(calculate PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()
