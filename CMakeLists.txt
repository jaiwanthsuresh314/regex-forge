cmake_minimum_required(VERSION 3.20)

project(regex_forge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(regex_forge
    src/lexer.cpp
    src/parser.cpp
    src/ast_printer.cpp
    src/backtracking.cpp
    src/analyzer.cpp
    src/strategy_selector.cpp
    src/runtime_monitor.cpp
    src/runtime_adaptation.cpp
)

target_include_directories(regex_forge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(regex_forge PUBLIC cxx_std_20)

enable_testing()
find_package(GTest CONFIG QUIET)

if (GTest_FOUND)
    add_executable(regex_forge_tests tests/lexer_test.cpp tests/parser_test.cpp tests/backtracking_test.cpp tests/analyzer_test.cpp tests/selector_test.cpp tests/runtime_adaptation_test.cpp tests/engine_contract_test.cpp)
    target_link_libraries(regex_forge_tests PRIVATE regex_forge GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(regex_forge_tests)
else()
    message(WARNING "GoogleTest was not found; configure with a GoogleTest installation to build tests.")
endif()
