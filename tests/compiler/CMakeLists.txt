add_executable(weasel_compiler_tests
    test_transpiler.cpp
)

target_link_libraries(weasel_compiler_tests PRIVATE weasel_compiler)
target_include_directories(weasel_compiler_tests PRIVATE ${doctest_SOURCE_DIR})

target_compile_definitions(weasel_compiler_tests PRIVATE
    WEASEL_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_test(NAME weasel_compiler_tests COMMAND weasel_compiler_tests)

# Smoke test: transpile task_list.weasel, compile, run, compare output with
# the hand-written reference task_list_example.
set(TASK_LIST_WEASEL "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/task_list.weasel")
set(TASK_LIST_CC     "${CMAKE_CURRENT_BINARY_DIR}/task_list_transpiled.cc")
add_custom_command(
    OUTPUT ${TASK_LIST_CC}
    COMMAND weaselc ${TASK_LIST_WEASEL} -o ${TASK_LIST_CC}
    DEPENDS weaselc ${TASK_LIST_WEASEL}
    VERBATIM
)
add_executable(task_list_transpiled ${TASK_LIST_CC})
target_link_libraries(task_list_transpiled PRIVATE weasel)

add_test(
    NAME task_list_smoke
    COMMAND ${CMAKE_COMMAND} -E compare_files
            $<TARGET_FILE:task_list_transpiled>.stdout
            $<TARGET_FILE:task_list_example>.stdout
)
add_test(
    NAME task_list_gather_reference
    COMMAND ${CMAKE_COMMAND} -E env
            sh -c "$<TARGET_FILE:task_list_example> > $<TARGET_FILE:task_list_example>.stdout"
)
add_test(
    NAME task_list_gather_transpiled
    COMMAND ${CMAKE_COMMAND} -E env
            sh -c "$<TARGET_FILE:task_list_transpiled> > $<TARGET_FILE:task_list_transpiled>.stdout"
)
set_tests_properties(task_list_smoke PROPERTIES
    DEPENDS "task_list_gather_reference;task_list_gather_transpiled")
