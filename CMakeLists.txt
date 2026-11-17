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

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia SerialPort Sql)
qt_standard_project_setup()

add_compile_options(-Wall -Wextra -Wpedantic)

qt_add_executable(qr-logger
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
)

target_include_directories(qr-logger PRIVATE src)
target_compile_definitions(qr-logger PRIVATE
    QRL_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS)

target_link_libraries(qr-logger PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    Qt6::Multimedia Qt6::SerialPort Qt6::Sql)

include(GNUInstallDirs)
install(TARGETS qr-logger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
