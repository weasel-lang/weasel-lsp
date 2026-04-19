add_library(weasel STATIC src/renderer.cpp)
target_include_directories(weasel PUBLIC include/)
target_compile_features(weasel PUBLIC cxx_std_20)

add_library(weasel_shared SHARED src/renderer.cpp)
set_target_properties(weasel_shared PROPERTIES OUTPUT_NAME weasel)
target_include_directories(weasel_shared PUBLIC include/)
target_compile_features(weasel_shared PUBLIC cxx_std_20)
