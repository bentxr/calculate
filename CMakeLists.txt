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
    src/entry.cpp
    src/keypad.cpp
    src/lcd.cpp
    src/mainwindow.cpp
    src/presenter.cpp
    src/typeset.cpp
    src/worker.cpp
)
target_include_directories(calculate-ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(calculate-ui PUBLIC Qt6::Widgets calculate-core)
target_compile_options(calculate-ui PRIVATE -Wall -Wextra -Wpedantic -Werror)
qt_add_translations(TARGETS calculate-ui TS_FILE_DIR i18n NO_GENERATE_PLURALS_TS_FILE)

qt_add_executable(calculate src/main.cpp)
target_link_libraries(calculate PRIVATE calculate-ui)
set_target_properties(calculate PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()
