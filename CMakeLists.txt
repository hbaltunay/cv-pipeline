cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0148)
    cmake_policy(SET CMP0148 NEW)
endif()

project(cv VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

set(OPTIMIZE "NONE" CACHE STRING "Optimize option: ON, OFF, or NONE")

#add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
#add_link_options(-fsanitize=address)

list(APPEND CMAKE_PREFIX_PATH 
    "/opt/civetweb"
    "/opt/spdlog"
    "/opt/nlohmann_json"
    "/opt/librdkafka"
)

find_package(civetweb REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(RdKafka REQUIRED)

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

find_library(TRITON_GRPC_LIB NAMES grpcclient)
find_library(TRITON_HTTP_LIB NAMES httpclient)

file(GLOB CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/core/*.cpp
    ${PROJECT_SOURCE_DIR}/src/gst/*.cpp
    ${PROJECT_SOURCE_DIR}/src/infer/*.cpp
    ${PROJECT_SOURCE_DIR}/src/cv/*.cpp
    ${PROJECT_SOURCE_DIR}/src/msg/*.cpp
    ${PROJECT_SOURCE_DIR}/src/server/*.cpp
    ${PROJECT_SOURCE_DIR}/src/utils/*.cpp
)

add_library(cv STATIC ${CORE_SOURCES})

if (OPTIMIZE STREQUAL "ON")
    target_compile_options(cv PRIVATE -O2 -fstack-usage)
    #add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF STACK_ANALYSIS)
elseif (OPTIMIZE STREQUAL "OFF")
    target_compile_options(cv PRIVATE -O0 -fstack-usage)
endif()

target_precompile_headers(cv PRIVATE <opencv2/core.hpp>)
target_compile_definitions(cv PRIVATE SPDLOG_COMPILED_LIB)

target_include_directories(cv PUBLIC
    ${OpenCV_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include/core
    ${PROJECT_SOURCE_DIR}/include/gst
    ${PROJECT_SOURCE_DIR}/include/infer
    ${PROJECT_SOURCE_DIR}/include/cv
    ${PROJECT_SOURCE_DIR}/include/msg
    ${PROJECT_SOURCE_DIR}/include/server
    ${PROJECT_SOURCE_DIR}/include/utils
)

target_link_libraries(cv PRIVATE
    civetweb::civetweb
    civetweb::civetweb-cpp
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${TRITON_GRPC_LIB}
    ${TRITON_HTTP_LIB}
    PkgConfig::GSTREAMER
    RdKafka::rdkafka++ 
    opencv_core
    opencv_videoio
    opencv_highgui
)

add_subdirectory(apps)
#add_subdirectory(tests)