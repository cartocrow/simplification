cmake_minimum_required(VERSION 3.16)

project(cartocrow-simplification
    VERSION 0.0.1
    DESCRIPTION "CartoCrow Simplification"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CartoCrow REQUIRED)

if(NOT EMSCRIPTEN)
    # Let CMake automatically handle Qt's "Meta-Object Compiler" preprocessor
    set(CMAKE_AUTOMOC ON)
endif()

# All source files should use include paths relative to the source root
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(library)
if(EMSCRIPTEN)
    add_subdirectory(wasm_frontend)
else()
    add_subdirectory(frontend)
    add_subdirectory(test)
endif()


