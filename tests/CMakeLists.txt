find_package(Qt6 REQUIRED COMPONENTS Test)
FetchContent_Declare(googletest
    URL      https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    SYSTEM)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(calculate-tests
    main.cpp
    test_devicekeyboard.cpp
    test_entry.cpp
    test_icons.cpp
    test_inspector.cpp
    test_keybutton.cpp
    test_keypad.cpp
    test_keysizing.cpp
    test_lcd.cpp
    test_licences.cpp
    test_mainwindow.cpp
    test_plot.cpp
    test_plotcanvas.cpp
    test_plotmode.cpp
    test_plotnav.cpp
    test_popupplacement.cpp
    test_presenter.cpp
    test_screenshots.cpp
    test_settings.cpp
    test_settingsfile.cpp
    test_translation.cpp
    test_typeset.cpp
    test_typing.cpp
    test_undostack.cpp
    test_worker.cpp
)
target_link_libraries(calculate-tests PRIVATE calculate-ui Qt6::Test GTest::gtest)
if(NOT APPLE)
    target_link_options(calculate-tests PRIVATE -Wl,--exclude-libs,ALL)
endif()
target_compile_definitions(calculate-tests PRIVATE CALCULATE_TS_FILE="${PROJECT_SOURCE_DIR}/i18n/calculate_es_ES.ts"
    CALCULATE_PLOT_FONTS="${calculate-plot_SOURCE_DIR}/fonts/ttf")

include(GoogleTest)
gtest_discover_tests(calculate-tests DISCOVERY_MODE PRE_TEST PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
