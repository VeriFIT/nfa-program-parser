if (MATA_INTERPRETERS)
    add_executable(debug-emp-interpreter
        interpreter_debug.cc)
    add_executable(mata-emp-interpreter
        interpreter_mata.cc)
    add_executable(mata-bdd-emp-interpreter
        interpreter_mata_bdd.cc)

    target_include_directories(debug-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})
    target_include_directories(mata-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})
    target_include_directories(mata-bdd-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})

    target_link_libraries(debug-emp-interpreter PUBLIC ${LIBMATA})
    target_link_libraries(mata-emp-interpreter PUBLIC ${LIBMATA})
    target_link_libraries(mata-bdd-emp-interpreter PUBLIC ${LIBMATA})
endif()

if (VATA_INTERPRETER)
    add_executable(vata-emp-interpreter
        interpreter_vata.cc)
    target_include_directories(vata-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR} ${VATA_INCLUDE_DIR})
    target_link_libraries(vata-emp-interpreter PUBLIC ${LIBMATA} ${LIBVATA} Boost::headers)
endif()

if (AWALI_INTERPRETER)
    add_executable(awali-emp-interpreter
        interpreter_awali.cc)
    target_include_directories(awali-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR} ${AWALI_INCLUDE_DIR})
    target_link_libraries(awali-emp-interpreter PUBLIC ${LIBMATA} ${LIBAWALI})
endif()

if (CUDD_INTERPRETER)
    add_executable(cudd-emp-interpreter
        interpreter_cudd.cc)
    target_include_directories(cudd-emp-interpreter PRIVATE ${CUDD_INCLUDE_DIR})
    target_link_libraries(cudd-emp-interpreter PUBLIC ${LIBCUDD})
endif()

if (MONA_INTERPRETER)
    add_executable(mona-emp-interpreter
        interpreter_mona.cc)
    target_include_directories(mona-emp-interpreter PRIVATE ${MONA_INCLUDE_DIR})
    target_link_libraries(mona-emp-interpreter PUBLIC ${LIBMONADFA} ${LIBMONABDD} ${LIBMONAMEM})
endif()
