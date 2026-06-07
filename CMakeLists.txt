cmake_minimum_required(VERSION 3.16)
project(BacktestingEngine VERSION 1.0.0 LANGUAGES CXX)

# Standard C++17 obligatoire (std::optional, structured bindings)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Flags d'optimisation et warnings
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra -Wpedantic")

# Mode Release par défaut si non spécifié
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Sources
set(SOURCES
    main.cpp
    src/DataParser.cpp
    src/Portfolio.cpp
    src/Indicators.cpp
    src/Optimizer.cpp
)

add_executable(backtest ${SOURCES})

target_include_directories(backtest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
