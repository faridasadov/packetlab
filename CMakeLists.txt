cmake_minimum_required(VERSION 3.21)

project(PacketLab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
endif()

set(PACKETLAB_SOURCES
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/topologyscene.cpp
    src/app/topologyscene.h
    src/core/device.cpp
    src/core/device.h
    src/core/networkinterface.cpp
    src/core/networkinterface.h
    src/core/link.cpp
    src/core/link.h
    src/core/networkmodel.cpp
    src/core/networkmodel.h
    src/core/projectserializer.cpp
    src/core/projectserializer.h
    src/core/simulator.cpp
    src/core/simulator.h
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(packetlab ${PACKETLAB_SOURCES})
else()
    add_executable(packetlab ${PACKETLAB_SOURCES})
endif()

target_include_directories(packetlab PRIVATE src)
target_link_libraries(packetlab PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

install(TARGETS packetlab RUNTIME DESTINATION bin)
