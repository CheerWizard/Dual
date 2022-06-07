cmake_minimum_required(VERSION 3.18.1)
project("dual")

add_library(${PROJECT_NAME} SHARED dual.cpp logger.h)

target_link_libraries(dual
        android
        log
        camera2ndk
)