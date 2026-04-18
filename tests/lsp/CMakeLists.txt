add_executable(weasel_lsp_tests
    test_lsp_server.cpp
)

target_link_libraries(weasel_lsp_tests PRIVATE weasel_lsp_core)
target_include_directories(weasel_lsp_tests PRIVATE ${doctest_SOURCE_DIR})

add_test(NAME weasel_lsp_tests COMMAND weasel_lsp_tests)
