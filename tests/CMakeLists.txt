include(FetchContent)

FetchContent_Declare(
    doctest
    URL      https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(weasel_tests
    test_node.cpp
    test_factory.cpp
    test_renderer.cpp
    test_context.cpp
)

target_link_libraries(weasel_tests PRIVATE weasel)
target_include_directories(weasel_tests PRIVATE ${doctest_SOURCE_DIR})

add_test(NAME weasel_tests COMMAND weasel_tests)

if(WEASEL_BUILD_COMPILER)
    add_subdirectory(compiler)
endif()

if(WEASEL_BUILD_LSP)
    add_subdirectory(lsp)
endif()
