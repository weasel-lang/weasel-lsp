add_executable(weasel_lsp_tests
    test_lsp_server.cpp
)

target_link_libraries(weasel_lsp_tests PRIVATE weasel_lsp_core)
target_include_directories(weasel_lsp_tests PRIVATE ${doctest_SOURCE_DIR})

add_test(NAME weasel_lsp_tests COMMAND weasel_lsp_tests)
set_tests_properties(weasel_lsp_tests PROPERTIES
    ENVIRONMENT "WEASEL_LSP_NO_CLANGD=1")

add_executable(mock_clangd mock_clangd.cpp)
target_link_libraries(mock_clangd PRIVATE nlohmann_json::nlohmann_json)

add_executable(weasel_lsp_proxy_tests test_clangd_proxy.cpp)
target_link_libraries(weasel_lsp_proxy_tests PRIVATE weasel_lsp_core)
target_include_directories(weasel_lsp_proxy_tests PRIVATE ${doctest_SOURCE_DIR})
add_dependencies(weasel_lsp_proxy_tests mock_clangd)

add_test(NAME weasel_lsp_proxy_tests COMMAND weasel_lsp_proxy_tests)
set_tests_properties(weasel_lsp_proxy_tests PROPERTIES
    ENVIRONMENT "WEASEL_CLANGD_PATH=$<TARGET_FILE:mock_clangd>")
