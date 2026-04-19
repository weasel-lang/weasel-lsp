include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

add_library(weasel_lsp_core STATIC
    src/jsonrpc.cpp
    src/document_store.cpp
    src/server.cpp
    src/completion.cpp
    src/definition.cpp
    src/diagnostics.cpp
    src/clangd_proxy.cpp
)
target_include_directories(weasel_lsp_core PUBLIC include/)

find_package(Threads REQUIRED)
target_link_libraries(weasel_lsp_core PUBLIC weasel_compiler nlohmann_json::nlohmann_json Threads::Threads)
target_compile_features(weasel_lsp_core PUBLIC cxx_std_20)

add_executable(weasel_lsp_server main.cpp)
target_link_libraries(weasel_lsp_server PRIVATE weasel_lsp_core)

install(TARGETS weasel_lsp_server RUNTIME DESTINATION bin)
