cmake_minimum_required(VERSION 3.16)

project(cartocrow-simplification
    VERSION 0.0.1
    DESCRIPTION "CartoCrow Simplification"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

find_package(CartoCrow REQUIRED)

# All source files should use include paths relative to the source root
include_directories(src)

add_subdirectory(library)
add_subdirectory(frontend)
add_subdirectory(test)


