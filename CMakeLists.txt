cmake_minimum_required(VERSION 3.21)

project(SerialTempMonitor
    VERSION 1.0.0
    DESCRIPTION "Serial temperature monitor built with Qt 6"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets SerialPort PrintSupport)

qt_standard_project_setup()

qt_add_executable(SerialTempMonitor WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    sample.h
    framecodec.cpp
    framecodec.h
    frameparser.cpp
    frameparser.h
    transport.h
    serialmanager.cpp
    serialmanager.h
    virtualserialport.cpp
    virtualserialport.h
    statistics.cpp
    statistics.h
    recorder.cpp
    recorder.h
    qcustomplot.cpp
    qcustomplot.h
)

target_include_directories(SerialTempMonitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SerialTempMonitor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::PrintSupport
)

# ---------------------------------------------------------------------------
# 协议单元测试：不依赖界面和串口，可直接在命令行运行。
# ---------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(tst_framecodec
        tests/tst_framecodec.cpp
        sample.h
        framecodec.cpp
        framecodec.h
        frameparser.cpp
        frameparser.h
    )

    target_include_directories(tst_framecodec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_framecodec PRIVATE Qt6::Core Qt6::Test)

    add_test(NAME tst_framecodec COMMAND tst_framecodec)

    qt_add_executable(tst_datamodel
        tests/tst_datamodel.cpp
        sample.h
        framecodec.cpp
        framecodec.h
        recorder.cpp
        recorder.h
        statistics.cpp
        statistics.h
        transport.h
        virtualserialport.cpp
        virtualserialport.h
    )

    target_include_directories(tst_datamodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_datamodel PRIVATE Qt6::Core Qt6::SerialPort Qt6::Test)

    add_test(NAME tst_datamodel COMMAND tst_datamodel)
endif()
