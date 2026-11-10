cmake_minimum_required(VERSION 3.10.2)

STRING(REGEX REPLACE ".*/(.*)" "\\1" CURRENT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
MESSAGE("current project: " ${CURRENT_FOLDER})

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CROSSCOMPILING TRUE)

cmake_host_system_information(RESULT arch_value QUERY OS_PLATFORM)
message(STATUS "architecture: " ${arch_value})

if((NOT "${arch_value}" STREQUAL "armv7l") AND (NOT "${arch_value}" STREQUAL "aarch64"))
    include ($ENV{HOME}/configs/cross.cmake)
endif()

project(${CURRENT_FOLDER})
add_definitions(-Wall -Wpointer-arith -DRV1126B)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -Wall -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")

# ======================== easyeai_api =========================
set(toolkit_root ${CMAKE_CURRENT_SOURCE_DIR}/../../easyeai-api)
set(common_root      ${toolkit_root}/common)
set(netProtocol_root ${toolkit_root}/netProtocol)
set(media_root       ${toolkit_root}/media)

include (${common_root}/log_manager_pro/api.cmake)
include (${common_root}/system_opt/api.cmake)
include (${netProtocol_root}/vcic/api.cmake)
include (${media_root}/rga/api.cmake)
include (${media_root}/display/api.cmake)

## GStreamer（server 端 mpp 硬件编码，client 端 mpp 硬件解码）
find_package(PkgConfig)
pkg_search_module(GST1.0 REQUIRED gstreamer-1.0)
pkg_search_module(GST1.0_APP REQUIRED gstreamer-app-1.0)
pkg_search_module(GST1.0_VIDEO REQUIRED gstreamer-video-1.0)

## api头文件路径
set(api_inc
    ${LOGMANAGER_PRO_INCLUDE_DIRS}
    ${SYSOPT_INCLUDE_DIRS}
    ${VCIC_INCLUDE_DIRS}
    ${RGA_INCLUDE_DIRS}
    ${DISPLAY_INCLUDE_DIRS}
    ${GST1.0_INCLUDE_DIRS}
    ${GST1.0_APP_INCLUDE_DIRS}
    ${GST1.0_VIDEO_INCLUDE_DIRS}
    ${CMAKE_SYSROOT}/usr/include/rk
    )
## api静态库文件路径
link_directories(
    ${LOGMANAGER_PRO_LIBS_DIRS}
    ${VCIC_LIBS_DIRS}
    ${DISPLAY_LIBS_DIRS}
    ${GST1.0_LIBRARY_DIRS}
    ${GST1.0_APP_LIBRARY_DIRS}
    ${GST1.0_VIDEO_LIBRARY_DIRS}
    ${CMAKE_SYSROOT}/usr/lib
    )
## api所依赖的库参数（server 和 client 共享）
set(sysLib_list
    ${VCIC_LIBS}
    ${SYSOPT_LIBS}
    ${LOGMANAGER_PRO_LIBS}
    )

#--------------------------
# vcic_stream_server
#--------------------------
set(demoTag vcic_stream_server)
add_executable(${demoTag}
    src/server/${demoTag}.cpp
    src/server/cameraCapturer.cpp
    src/server/camera/mipi_camera.c
    src/server/fileCapturer.cpp
    )
target_link_libraries(${demoTag}
    ${sysLib_list}
    ${RGA_LIBS}
    ${GST1.0_LIBRARIES}
    ${GST1.0_APP_LIBRARIES}
    )
target_include_directories(${demoTag} PRIVATE
    ${api_inc}
    src/server
    )

#--------------------------
# vcic_stream_client
#--------------------------
set(demoTag vcic_stream_client)
add_executable(${demoTag}
    src/client/${demoTag}.cpp
    src/client/gstDecoder.cpp
    ${RGA_SOURCE_DIRS}
    )
target_link_libraries(${demoTag}
    ${sysLib_list}
    ${DISPLAY_LIBS}
    ${RGA_LIBS}
    ${GST1.0_LIBRARIES}
    ${GST1.0_APP_LIBRARIES}
    ${GST1.0_VIDEO_LIBRARIES}
    )
target_include_directories(${demoTag} PRIVATE
    ${api_inc}
    src/client
    )
