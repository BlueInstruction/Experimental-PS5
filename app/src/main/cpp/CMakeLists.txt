cmake_minimum_required(VERSION 3.22.1)

project("px5")

add_library(
        px5
        SHARED
        fexcore_wrapper.cpp
        turnip_hook.cpp)

find_library(
        log-lib
        log)

target_link_libraries(
        px5
        ${log-lib})
