add_executable(debug-emp-interpreter
    interpreter_debug.cc)
add_executable(mata-emp-interpreter
    interpreter_mata.cc)

target_include_directories(debug-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})
target_include_directories(mata-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})

target_link_libraries(debug-emp-interpreter PUBLIC ${LIBMATA})
target_link_libraries(mata-emp-interpreter PUBLIC ${LIBMATA})

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
