cmake_minimum_required(VERSION 3.4.1)

find_library(camera-lib camera2ndk)
find_library(media-lib mediandk)
find_library(android-lib android)
find_library(gl-lib GLESv2)

add_library(jniapi SHARED
        jniapi.cpp
        cam_utils.cpp)

target_link_libraries(jniapi
        android
        log
        ${camera-lib}
        ${media-lib}
        ${android-lib}
        ${gl-lib})
