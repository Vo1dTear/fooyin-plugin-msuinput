cmake_minimum_required(VERSION 3.16)
project(msuinput VERSION 0.2.0 LANGUAGES CXX)

add_subdirectory(msuinput)

option(MSU_BUILD_TESTS "Build MSU decoder regression tests" OFF)
if(MSU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
