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
option(WEASEL_BUILD_COMPILER "Build the weasel compiler"  ON)
option(WEASEL_BUILD_LSP      "Build the weasel LSP server" ON)

if(WEASEL_BUILD_COMPILER)
    add_subdirectory(compiler)
endif()

if(WEASEL_BUILD_LSP)
    if(NOT WEASEL_BUILD_COMPILER)
        message(FATAL_ERROR "WEASEL_BUILD_LSP requires WEASEL_BUILD_COMPILER")
    endif()
    add_subdirectory(lsp)
endif()

if(WEASEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WEASEL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Regenerate all committed .cc files from their .weasel sources. Useful after
# editing the emitter or any parser logic: run `cmake --build build --target
# weasel_regenerate` and review the resulting .cc diffs before committing.
if(WEASEL_BUILD_COMPILER)
    file(GLOB_RECURSE WEASEL_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/*.weasel")
    add_custom_target(weasel_regenerate)
    foreach(WEASEL_SRC IN LISTS WEASEL_SOURCES)
        get_filename_component(WEASEL_DIR ${WEASEL_SRC} DIRECTORY)
        get_filename_component(WEASEL_NAME ${WEASEL_SRC} NAME_WE)
        set(WEASEL_CC "${WEASEL_DIR}/${WEASEL_NAME}.cc")
        add_custom_command(TARGET weasel_regenerate POST_BUILD
            COMMAND weaselc ${WEASEL_SRC} -o ${WEASEL_CC}
            DEPENDS ${WEASEL_SRC}
            COMMENT "regenerating ${WEASEL_CC}"
            VERBATIM)
    endforeach()
endif()
