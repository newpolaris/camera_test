cmake_minimum_required(VERSION 3.4.1)

add_library(jniapi SHARED
            jniapi.cpp)

target_link_libraries(jniapi
                      android
                      log)
