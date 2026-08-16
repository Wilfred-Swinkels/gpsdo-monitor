cmake_minimum_required(VERSION 3.16)
project(gpsdo_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core SerialPort Quick)

# Kernlogica (FllLink + Mcp3426Adc + GpsLink + Tsic506Driver), los van elke
# UI — kan straks zowel door het CLI-smoke-test-programma hieronder als
# door de echte QML-app gebruikt worden.
add_library(gpsdo_core STATIC
    src/FllLink.h
    src/FllLink.cpp
    src/Mcp3426Adc.h
    src/Mcp3426Adc.cpp
    src/GpsLink.h
    src/GpsLink.cpp
    src/Tsic506Driver.h
    src/Tsic506Driver.cpp
)
target_link_libraries(gpsdo_core PUBLIC Qt6::Core Qt6::SerialPort pigpio pthread)
target_include_directories(gpsdo_core PUBLIC src)

# Klein CLI-programma zonder UI, om FllLink/Mcp3426Adc/GpsLink/Tsic506Driver
# tegen echte hardware te verifiëren vóór de QML-app er staat. Zie
# README.md voor gebruik.
add_executable(gpsdo_test_cli src/main_test_cli.cpp)
target_link_libraries(gpsdo_test_cli PRIVATE gpsdo_core)

# De echte QML-app. QML wordt bewust rechtstreeks van schijf geladen
# (QUrl::fromLocalFile in app/main.cpp) i.p.v. via het Qt Resource System —
# zie app/main.cpp voor de motivatie. QML_DIR geeft main.cpp het absolute
# pad naar de qml/-map in de broncode, zodat dat nooit hard hoeft te worden
# gecodeerd en gewoon meebeweegt als de repo ergens anders geclonet wordt.
add_executable(gpsdo_app
    app/main.cpp
    app/GpsdoModel.h
    app/GpsdoModel.cpp
    app/TimeModeSupervisor.h
    app/TimeModeSupervisor.cpp
)
target_link_libraries(gpsdo_app PRIVATE gpsdo_core Qt6::Quick)
target_include_directories(gpsdo_app PRIVATE app)
target_compile_definitions(gpsdo_app PRIVATE QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
