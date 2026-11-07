cmake_minimum_required(VERSION 3.16)
project(MiniCppParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Flex converts lexer.l into C++ source; Bison converts parser.y into C++ source.
find_package(FLEX 2.6 REQUIRED)
find_package(BISON 3.6 REQUIRED)

BISON_TARGET(
    MiniCppGrammar
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
)

FLEX_TARGET(
    MiniCppScanner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.cpp
)

ADD_FLEX_BISON_DEPENDENCY(MiniCppScanner MiniCppGrammar)

add_executable(mini_cpp_parser
    src/main.cpp
    src/ast.cpp
    src/interpreter.cpp
    src/python_generator.cpp
    ${BISON_MiniCppGrammar_OUTPUTS}
    ${FLEX_MiniCppScanner_OUTPUTS}
)

target_include_directories(mini_cpp_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

if (MSVC)
    target_compile_options(mini_cpp_parser PRIVATE /W4)
else()
    target_compile_options(mini_cpp_parser PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Keep the executable in build/ on both single- and multi-configuration generators.
set_target_properties(mini_cpp_parser PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(mini_cpp_parser PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}"
    )
endforeach()
