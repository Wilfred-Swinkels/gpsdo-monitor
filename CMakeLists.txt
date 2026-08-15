cmake_minimum_required(VERSION 3.16)
project(gpsdo_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

# Kernlogica (FllLink + Mcp3426Adc), los van elke UI — kan straks zowel door
# het CLI-smoke-test-programma hieronder als door de echte QML-app gebruikt
# worden.
add_library(gpsdo_core STATIC
    src/FllLink.h
    src/FllLink.cpp
    src/Mcp3426Adc.h
    src/Mcp3426Adc.cpp
)
target_link_libraries(gpsdo_core PUBLIC Qt6::Core Qt6::SerialPort)
target_include_directories(gpsdo_core PUBLIC src)

# Klein CLI-programma zonder UI, om FllLink/Mcp3426Adc tegen echte hardware
# te verifiëren vóór de QML-app er staat. Zie README.md voor gebruik.
add_executable(gpsdo_test_cli src/main_test_cli.cpp)
target_link_libraries(gpsdo_test_cli PRIVATE gpsdo_core)
