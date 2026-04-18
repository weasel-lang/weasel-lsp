cmake_minimum_required(VERSION 3.20)
project(weasel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(weasel STATIC src/renderer.cpp)
target_include_directories(weasel PUBLIC include/)
target_compile_features(weasel PUBLIC cxx_std_20)

option(WEASEL_BUILD_TESTS    "Build the weasel test suite" ON)
option(WEASEL_BUILD_EXAMPLES "Build the weasel examples"  ON)

if(WEASEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WEASEL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
