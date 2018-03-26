cmake_minimum_required(VERSION 2.8)

project(get_h264)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

set(GENICAM_V2_4 "/opt/genicam/bin/Linux64_x64")
set(KAYA_INSTRUMENTS "/opt/KAYA_Instruments")
set(OPENCV_V2_4 "/opt/opencv")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

include_directories(${KAYA_INSTRUMENTS}/include)
link_directories( ${GENICAM_V2_4} ${KAYA_INSTRUMENTS}/lib ${OPENCV_V2_4} )

#find_library (
#    X264_LIB
#    NAMES libx264.so.142
#    PATHS /usr/lib/x86_64-linux-gnu
#)

#find_library(X264_LIB libx264.a)
find_library(X264_LIB x264)

FILE(GLOB_RECURSE INCLUDES "*.h")
add_custom_target(headers SOURCES ${INCLUDES})
set(SOURCES "main.cpp" "camera.cpp" "converter.cpp" "encoder.cpp" "bayer.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} pthread KYFGLib ${X264_LIB})
#target_link_libraries(get_h264 pthread x264 KYFGLib)
