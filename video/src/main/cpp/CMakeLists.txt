#cmake_minimum_required(VERSION 3.10.2)
##file(GLOB SOURCE ${CMAKE_SOURCE_DIR} )
#AUX_SOURCE_DIRECTORY(${CMAKE_SOURCE_DIR} SRC_LIST)
#add_library(
#        native-lib
#        SHARED
#        ${SRC_LIST}
#)
##LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs/armeabi-v7a)
#set(my_lib_path ${CMAKE_SOURCE_DIR}/libs/armeabi-v7a)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${my_lib_path} ")
#
#
#include_directories(${CMAKE_SOURCE_DIR}/include)
#target_link_libraries(
#        native-lib
#        avfilter avformat    avcodec  avutil  swresample swscale
#        z
#        OpenSLES
#        android
#        log )


# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

project("myapplication")

set(lib_src_DIR ${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI})

#在我的电脑上，这种直接连接动态库不行了，baidu上说可以，官网上也不不推荐这个，推荐使用 add_library 和 set_target_properties
#link_directories(${CMAKE_SOURCE_DIR}/ffmpeg/libs/armeabi-v7a/)

#静态库可以用以下直接设置链接静态库文件
#set(my_lib_path ${CMAKE_SOURCE_DIR}/ffmpeg/libs/armeabi-v7a)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${my_lib_path} ")

add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libavcodec.so)

add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libavfilter.so)

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libavformat.so)

add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libavutil.so)

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libswresample.so)

add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION
        ${lib_src_DIR}/libswscale.so)

add_library( # Sets the name of the library.
        native-lib

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp FFmpegPlayer.cpp JavaCallHelper.cpp VideoChannel.cpp)


find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log )


include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR})
target_link_libraries( # Specifies the target library.
        native-lib
        avcodec
        avfilter
        avformat
        avutil
        swresample
        swscale
        android
        OpenSLES
        z
        ${log-lib} )