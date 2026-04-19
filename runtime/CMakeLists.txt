add_library(weasel STATIC src/renderer.cpp)
target_include_directories(weasel PUBLIC include/)
target_compile_features(weasel PUBLIC cxx_std_20)
