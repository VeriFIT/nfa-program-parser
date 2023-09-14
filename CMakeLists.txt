cmake_minimum_required (VERSION 3.0.0)
project (nfa-emp-interpreter)

set (CMAKE_CXX_STANDARD 20)

find_library(LIBMATA mata REQUIRED)
find_library(LIBAWALI awalidyn REQUIRED)
find_library(LIBMONADFA monadfa REQUIRED)
find_library(LIBMONAMEM monamem REQUIRED)
find_library(LIBMONABDD monabdd REQUIRED)

add_subdirectory(libvata)
add_subdirectory(src/cpp)