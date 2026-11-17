# SPDX-License-Identifier: GPL-3.0-or-later
# Mindestbasis: Debian 12 (CMake 3.25, Qt 6.4.2, GCC 12) / Ubuntu 24.04
cmake_minimum_required(VERSION 3.25)

project(qr-logger
    VERSION 0.1.0
    DESCRIPTION "Contest logger for FlexRadio (SmartSDR API) on Linux"
    HOMEPAGE_URL "https://github.com/hberkenfeld/qr-logger"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QRL_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia SerialPort Sql)
qt_standard_project_setup()

add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_definitions(QRL_VERSION="${PROJECT_VERSION}" QT_NO_KEYWORDS)

# --- Core library (no GUI) ---------------------------------------------------
qt_add_library(qrl_core STATIC
    src/radio/flex/FlexProtocol.cpp     src/radio/flex/FlexProtocol.h
    src/radio/flex/FlexDiscovery.cpp    src/radio/flex/FlexDiscovery.h
    src/radio/flex/FlexConnection.cpp   src/radio/flex/FlexConnection.h
    src/radio/flex/FlexRadio.cpp        src/radio/flex/FlexRadio.h
    src/contest/Qso.h
    src/contest/Bands.cpp               src/contest/Bands.h
    src/contest/Contest.h
    src/contest/TtcContest.cpp          src/contest/TtcContest.h
    src/log/LogDatabase.cpp             src/log/LogDatabase.h
    src/log/Cabrillo.cpp                src/log/Cabrillo.h
    src/tx/Macros.cpp                   src/tx/Macros.h
    src/tx/CwKeyer.cpp                  src/tx/CwKeyer.h
    src/net/ClusterClient.cpp           src/net/ClusterClient.h
)
target_include_directories(qrl_core PUBLIC src)
target_link_libraries(qrl_core PUBLIC Qt6::Core Qt6::Network Qt6::Sql)

# --- Application ---------------------------------------------------------------
qt_add_executable(qr-logger
    src/main.cpp
    src/app/MainWindow.cpp      src/app/MainWindow.h
    src/app/ConnectDialog.cpp   src/app/ConnectDialog.h
    src/app/RadioPanel.cpp      src/app/RadioPanel.h
    src/app/ContestSetupDialog.cpp src/app/ContestSetupDialog.h
    src/app/MessagesDialog.cpp  src/app/MessagesDialog.h
    src/ui/ContestPanel.cpp     src/ui/ContestPanel.h
    src/audio/Sidetone.cpp      src/audio/Sidetone.h
    src/ui/FreeTextDialog.cpp   src/ui/FreeTextDialog.h
    src/ui/BandmapPanel.cpp     src/ui/BandmapPanel.h
    src/app/ClusterSettingsDialog.cpp src/app/ClusterSettingsDialog.h
)
target_link_libraries(qr-logger PRIVATE
    qrl_core
    Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::SerialPort Qt6::Sql)

# --- Tools ---------------------------------------------------------------------
qt_add_executable(qrl-flexprobe src/tools/flexprobe.cpp)
target_link_libraries(qrl-flexprobe PRIVATE qrl_core)

include(GNUInstallDirs)
install(TARGETS qr-logger qrl-flexprobe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Tests ---------------------------------------------------------------------
if(QRL_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(tst_flexprotocol tests/tst_flexprotocol.cpp)
    target_link_libraries(tst_flexprotocol PRIVATE qrl_core Qt6::Test)
    add_test(NAME tst_flexprotocol COMMAND tst_flexprotocol)
    qt_add_executable(tst_contest tests/tst_contest.cpp src/audio/Sidetone.cpp src/audio/Sidetone.h)
    target_link_libraries(tst_contest PRIVATE qrl_core Qt6::Multimedia Qt6::Test)
    add_test(NAME tst_contest COMMAND tst_contest)
    qt_add_executable(tst_operating tests/tst_operating.cpp
        src/ui/ContestPanel.cpp src/ui/ContestPanel.h
        src/ui/FreeTextDialog.cpp src/ui/FreeTextDialog.h
        src/ui/BandmapPanel.cpp src/ui/BandmapPanel.h)
    target_compile_definitions(tst_operating PRIVATE QRL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(tst_operating PRIVATE qrl_core Qt6::Widgets Qt6::Test)
    add_test(NAME tst_operating COMMAND tst_operating)
endif()
