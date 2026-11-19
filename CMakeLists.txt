cmake_minimum_required(VERSION 3.20)

project(SPL VERSION 0.1 LANGUAGES CXX)

add_library(spl_core STATIC
    src/spl.cpp
)
target_compile_features(spl_core PUBLIC cxx_std_20)
target_include_directories(spl_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(spl_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(spl_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(spl_test tests/main.cpp)
target_link_libraries(spl_test PRIVATE spl_core)

enable_testing()
add_test(NAME spl_core_test COMMAND spl_test)
