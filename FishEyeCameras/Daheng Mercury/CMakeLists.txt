cmake_minimum_required(VERSION 2.8)

project(get_h264)

set(GENICAM_ROOT_V2_3 "/home/brix2/U3-driver/dhmercam_install_en_20170705/dh_mer_camera/daheng-sdk-x64/sdk")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

add_library(gxiapi SHARED IMPORTED)
set_target_properties(gxiapi PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/lib/libgxiapi.so"
  INTERFACE_INCLUDE_DIRECTORIES "${GENICAM_ROOT_V2_3}/include"
)
add_library(dximageproc SHARED IMPORTED)
set_target_properties(dximageproc PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/lib/libdximageproc.so"
)
add_library(GCBase_gcc40_v2_3 SHARED IMPORTED)
set_target_properties(GCBase_gcc40_v2_3 PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/genicam/bin/Linux64_x64/libGCBase_gcc40_v2_3.so"
)
add_library(GenApi_gcc40_v2_3 SHARED IMPORTED)
set_target_properties(GenApi_gcc40_v2_3 PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/genicam/bin/Linux64_x64/libGenApi_gcc40_v2_3.so"
  INTERFACE_INCLUDE_DIRECTORIES "${GENICAM_ROOT_V2_3}/genicam/library/CPP/include"
)
add_library(log4cpp_gcc40_v2_3 SHARED IMPORTED)
set_target_properties(log4cpp_gcc40_v2_3 PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/genicam/bin/Linux64_x64/liblog4cpp_gcc40_v2_3.so"
)
add_library(Log_gcc40_v2_3 SHARED IMPORTED)
set_target_properties(Log_gcc40_v2_3 PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/genicam/bin/Linux64_x64/libLog_gcc40_v2_3.so"
)
add_library(MathParser_gcc40_v2_3 SHARED IMPORTED)
set_target_properties(MathParser_gcc40_v2_3 PROPERTIES
  IMPORTED_LOCATION "${GENICAM_ROOT_V2_3}/genicam/bin/Linux64_x64/libMathParser_gcc40_v2_3.so"
)

FILE(GLOB_RECURSE INCLUDES "*.h")
add_custom_target(headers SOURCES ${INCLUDES})
set(SOURCES "main.cpp" "converter.cpp" "camera.cpp" "encoder.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(get_h264 pthread x264 gxiapi dximageproc GCBase_gcc40_v2_3 GenApi_gcc40_v2_3 log4cpp_gcc40_v2_3 Log_gcc40_v2_3 MathParser_gcc40_v2_3)
