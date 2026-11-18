cmake_minimum_required (VERSION 3.5.0)
project (nfa-emp-interpreter)

# Mata's headers have used C++23 since v1.32.45-dev.21.
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# The interpreters on mata (mata, mata-bdd, debug) are built by default. The
# others are opt-in, each finding its library like libmata (e.g. through
# CMAKE_PREFIX_PATH); vata and awali read their inputs with libmata too, cudd
# and mona do not need it.
option(MATA_INTERPRETERS "Build the interpreters on mata (mata, mata-bdd, debug)" ON)
option(VATA_INTERPRETER "Build vata-emp-interpreter (needs libvata and Boost headers)" OFF)
option(AWALI_INTERPRETER "Build awali-emp-interpreter (needs awali's libawalidyn)" OFF)
option(CUDD_INTERPRETER "Build cudd-emp-interpreter (needs CUDD's libcudd)" OFF)
option(MONA_INTERPRETER "Build mona-emp-interpreter (needs MONA's libmonadfa, libmonabdd and libmonamem)" OFF)
if (MATA_INTERPRETERS OR VATA_INTERPRETER OR AWALI_INTERPRETER)
    find_library(LIBMATA mata REQUIRED)
    find_path(MATA_INCLUDE_DIR NAMES mata/nfa/nfa.hh REQUIRED)
endif()
if (VATA_INTERPRETER)
    find_library(LIBVATA vata REQUIRED)
    find_path(VATA_INCLUDE_DIR NAMES vata/explicit_tree_aut.hh REQUIRED)
    find_package(Boost REQUIRED CONFIG)
endif()
if (AWALI_INTERPRETER)
    find_library(LIBAWALI awalidyn REQUIRED)
    find_path(AWALI_INCLUDE_DIR NAMES awali/dyn.hh REQUIRED)
endif()
if (CUDD_INTERPRETER)
    find_library(LIBCUDD cudd REQUIRED)
    find_path(CUDD_INCLUDE_DIR NAMES cudd.h REQUIRED)
endif()
if (MONA_INTERPRETER)
    find_library(LIBMONADFA monadfa REQUIRED)
    find_library(LIBMONABDD monabdd REQUIRED)
    find_library(LIBMONAMEM monamem REQUIRED)
    find_path(MONA_INCLUDE_DIR NAMES mona/dfa.h REQUIRED)
endif()

add_subdirectory(src/cpp)
