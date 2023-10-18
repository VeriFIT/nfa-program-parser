cmake_minimum_required (VERSION 3.5.0)
project (nfa-emp-interpreter)

set (CMAKE_CXX_STANDARD 20)

find_library(LIBMATA mata REQUIRED)
find_library(LIBAWALI awalidyn REQUIRED)

add_subdirectory(libvata)
add_subdirectory(src/cpp)