cmake_minimum_required(VERSION 3.25)
project(calculate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

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
    src/keypad.cpp
    src/presenter.cpp
)
target_include_directories(calculate-ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(calculate-ui PUBLIC Qt6::Widgets calculate-core)
target_compile_options(calculate-ui PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()
