cmake_minimum_required(VERSION 3.20)

project(SPL VERSION 0.1 LANGUAGES CXX)

find_package(Python3 COMPONENTS Development.Embed REQUIRED)

add_library(spl_core STATIC
    src/Artifact.cpp
    src/Parser.cpp
    src/parser/Compiler.cpp
    src/parser/Lexer.cpp
    src/parser/SyntaxParser.cpp
    src/spl.cpp
    src/runtime/PythonAdapter.cpp
)
target_compile_features(spl_core PUBLIC cxx_std_20)
target_include_directories(spl_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(spl_core PUBLIC Python3::Python)

if (MSVC)
    target_compile_options(spl_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(spl_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(SPL_TEST_TARGETS
    spl_value_test
    spl_graph_test
    spl_error_test
    spl_parser_test
    spl_artifact_test
    spl_python_runtime_test
)

add_executable(spl_value_test tests/value_test.cpp)
add_executable(spl_graph_test tests/graph_test.cpp)
add_executable(spl_error_test tests/error_test.cpp)
add_executable(spl_parser_test tests/parser_test.cpp)
add_executable(spl_artifact_test tests/artifact_test.cpp)
add_executable(spl_python_runtime_test tests/python_runtime_test.cpp)

foreach(test_target IN LISTS SPL_TEST_TARGETS)
    target_link_libraries(${test_target} PRIVATE spl_core)
    target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endforeach()

add_executable(spl-run src/cli/main.cpp)
target_link_libraries(spl-run PRIVATE spl_core)

enable_testing()
add_test(NAME spl_value COMMAND spl_value_test)
add_test(NAME spl_graph COMMAND spl_graph_test)
add_test(NAME spl_error COMMAND spl_error_test)
add_test(NAME spl_parser COMMAND spl_parser_test)
add_test(NAME spl_artifact COMMAND spl_artifact_test)
add_test(NAME spl_python_runtime COMMAND spl_python_runtime_test)
add_test(NAME spl_python_example COMMAND spl-run ${CMAKE_CURRENT_SOURCE_DIR}/examples/pipeline.spl)
set_tests_properties(
    spl_value spl_graph spl_error spl_parser spl_artifact spl_python_runtime spl_python_example
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
