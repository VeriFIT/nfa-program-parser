cmake_minimum_required (VERSION 3.5.0)
project (nfa-emp-interpreter)

set (CMAKE_CXX_STANDARD 20)

find_library(LIBMATA mata REQUIRED)
find_path(MATA_INCLUDE_DIR NAMES mata/nfa/nfa.hh REQUIRED)

# Competitors' interpreters are opt-in, so that building mata's needs neither
# library. Both are found like libmata (e.g. through CMAKE_PREFIX_PATH).
option(VATA_INTERPRETER "Build vata-emp-interpreter (needs libvata and Boost headers)" OFF)
option(AWALI_INTERPRETER "Build awali-emp-interpreter (needs awali's libawalidyn)" OFF)
if (VATA_INTERPRETER)
    find_library(LIBVATA vata REQUIRED)
    find_path(VATA_INCLUDE_DIR NAMES vata/explicit_tree_aut.hh REQUIRED)
    find_package(Boost REQUIRED CONFIG)
endif()
if (AWALI_INTERPRETER)
    find_library(LIBAWALI awalidyn REQUIRED)
    find_path(AWALI_INCLUDE_DIR NAMES awali/dyn.hh REQUIRED)
endif()

# find_library(LIBMONADFA monadfa REQUIRED)
# find_library(LIBMONAMEM monamem REQUIRED)
# find_library(LIBMONABDD monabdd REQUIRED)

add_subdirectory(src/cpp)
