add_library(weasel_compiler STATIC
    src/source.cpp
    src/scanner.cpp
    src/jsx_parser.cpp
    src/emitter.cpp
    src/transpiler.cpp
)
target_include_directories(weasel_compiler PUBLIC include/)
target_compile_features(weasel_compiler PUBLIC cxx_std_20)

add_executable(weaselc src/main.cpp)
target_link_libraries(weaselc PRIVATE weasel_compiler)
