cmake_minimum_required(VERSION 3.16)
project(BacktestingEngine VERSION 2.0.0 LANGUAGES CXX)

option(BACKTEST_BUILD_TESTS "Build the unit tests (GoogleTest)" ON)
option(BACKTEST_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BACKTEST_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# --------------------------------------------------------------------------- #
# Compiler options shared by every project target (not by third-party code)
# --------------------------------------------------------------------------- #
add_library(backtest_options INTERFACE)
target_compile_features(backtest_options INTERFACE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(backtest_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor
        -Woverloaded-virtual -Wold-style-cast -Wnull-dereference)
    if(BACKTEST_WARNINGS_AS_ERRORS)
        target_compile_options(backtest_options INTERFACE -Werror)
    endif()
    if(BACKTEST_SANITIZERS)
        target_compile_options(backtest_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(backtest_options INTERFACE -fsanitize=address,undefined)
    endif()
elseif(MSVC)
    target_compile_options(backtest_options INTERFACE /W4 /permissive-)
    if(BACKTEST_WARNINGS_AS_ERRORS)
        target_compile_options(backtest_options INTERFACE /WX)
    endif()
endif()

# --------------------------------------------------------------------------- #
# Engine library (everything but main), reused by the executable and the tests
# --------------------------------------------------------------------------- #
add_library(backtest_core
    src/Backtester.cpp
    src/Cli.cpp
    src/DataParser.cpp
    src/Indicators.cpp
    src/Metrics.cpp
    src/Optimizer.cpp
    src/Report.cpp
    src/strategies/MovingAverageCrossover.cpp
    src/strategies/RSIStrategy.cpp
)
target_include_directories(backtest_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(backtest_core
    PUBLIC Threads::Threads
    PRIVATE backtest_options)
# std::filesystem needs an extra library with GCC 8.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(backtest_core PUBLIC stdc++fs)
endif()

add_executable(backtest main.cpp)
target_link_libraries(backtest PRIVATE backtest_core backtest_options)

# --------------------------------------------------------------------------- #
# Tests
# --------------------------------------------------------------------------- #
if(BACKTEST_BUILD_TESTS)
    enable_testing()

    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        if(POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()
        FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
            URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_subdirectory(tests)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Tests: ${BACKTEST_BUILD_TESTS} | Werror: ${BACKTEST_WARNINGS_AS_ERRORS} | Sanitizers: ${BACKTEST_SANITIZERS}")
