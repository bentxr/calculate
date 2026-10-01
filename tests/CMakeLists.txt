find_package(Qt6 REQUIRED COMPONENTS Test)
FetchContent_Declare(googletest
    URL      https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    SYSTEM)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(calculate-tests
    main.cpp
    test_presenter.cpp
)
target_link_libraries(calculate-tests PRIVATE calculate-ui Qt6::Test GTest::gtest)

include(GoogleTest)
gtest_discover_tests(calculate-tests DISCOVERY_MODE PRE_TEST PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
