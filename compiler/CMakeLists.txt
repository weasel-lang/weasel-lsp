configure_file(
    include/weasel/compiler/version.hpp.in
    include/weasel/compiler/version.hpp
    @ONLY
)

add_library(weasel_compiler STATIC
    src/source.cpp
    src/scanner.cpp
    src/ccx_parser.cpp
    src/emitter.cpp
    src/transpiler.cpp
    src/boundary.cpp
)
target_include_directories(weasel_compiler PUBLIC include/)
target_compile_features(weasel_compiler PUBLIC cxx_std_20)

add_executable(weaselc src/main.cpp)
target_link_libraries(weaselc PRIVATE weasel_compiler)
target_include_directories(weaselc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

install(TARGETS weaselc RUNTIME DESTINATION bin)
