# =============================================================================
#  SemiEqMonitor —— 半导体设备数据采集与监控上位机
#
#  构建：
#     cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt5_DIR>
#     cmake --build build
#
#  运行（MinGW 需先把 Qt 的 bin 与编译器的 bin 加进 PATH）：
#     scripts\run.bat
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(SemiEqMonitor
    VERSION 0.1.0
    DESCRIPTION "Semiconductor equipment data acquisition and monitoring HMI"
    LANGUAGES CXX)

# ------------------------------------------------------------------ 编译选项
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(SEMIEQ_BUILD_TESTS "构建单元测试" ON)

# ------------------------------------------------------------------ Qt 依赖
find_package(Qt5 5.14 REQUIRED COMPONENTS Core Widgets Network Sql SerialPort)

# ----------------------------------------------------- 生成版本头文件（唯一来源）
# 版本号只在 project() 里写一次，代码里用 SEMIEQ_VERSION_STR 读取
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/common/version.h
    @ONLY)

# ======================================================== semieq_core 静态库
# 采集层 + 持久层 + 公共设施。被上位机主程序与单元测试复用。
set(SEMIEQ_CORE_SOURCES
    src/common/logger.cpp
    src/acquisition/modbustcpclient.cpp
    src/data/databasemanager.cpp)

set(SEMIEQ_CORE_HEADERS
    src/common/types.h
    src/common/logger.h
    src/acquisition/ideviceclient.h
    src/acquisition/modbustcpclient.h
    src/data/databasemanager.h)

add_library(semieq_core STATIC ${SEMIEQ_CORE_SOURCES} ${SEMIEQ_CORE_HEADERS})

target_include_directories(semieq_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_link_libraries(semieq_core
    PUBLIC Qt5::Core Qt5::Network Qt5::Sql)

if(MSVC)
    target_compile_options(semieq_core PRIVATE /W4)
else()
    target_compile_options(semieq_core PRIVATE -Wall -Wextra)
endif()

# ==================================================== 上位机主程序（HMI）
set(SEMIEQ_APP_SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h)

add_executable(SemiEqMonitor ${SEMIEQ_APP_SOURCES})

target_link_libraries(SemiEqMonitor
    PRIVATE semieq_core Qt5::Widgets Qt5::SerialPort)

# ================================================= 设备模拟器（独立可执行程序）
# 刻意不依赖 semieq_core：它扮演"设备侧"，与上位机是两套独立软件
add_executable(SemiEqSimulator
    apps/simulator/main.cpp
    apps/simulator/devicesimulator.cpp
    apps/simulator/devicesimulator.h)

target_link_libraries(SemiEqSimulator PRIVATE Qt5::Core Qt5::Network)

# ============================================================ 单元测试
if(SEMIEQ_BUILD_TESTS)
    find_package(Qt5 REQUIRED COMPONENTS Test)
    enable_testing()

    add_executable(semieq_tests tests/test_modbus.cpp)
    target_link_libraries(semieq_tests PRIVATE semieq_core Qt5::Test)

    add_test(NAME semieq_tests COMMAND semieq_tests)
endif()

# ============================================================ 构建摘要
message(STATUS "")
message(STATUS "  ===============================================")
message(STATUS "   SemiEqMonitor  v${PROJECT_VERSION}")
message(STATUS "   Qt 位置   : ${Qt5_DIR}")
message(STATUS "   构建类型  : ${CMAKE_BUILD_TYPE}")
message(STATUS "   单元测试  : ${SEMIEQ_BUILD_TESTS}")
message(STATUS "   输出目录  : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  ===============================================")
message(STATUS "")
