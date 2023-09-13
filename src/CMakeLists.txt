add_executable(debug-emp-interpreter
    interpreter_debug.cc)
add_executable(mata-emp-interpreter
    interpreter_mata.cc)
add_executable(vata-emp-interpreter
    interpreter_vata.cc)
add_executable(awali-emp-interpreter
    interpreter_awali.cc)
add_executable(mona-emp-interpreter
    interpreter_mona.cc)

target_link_libraries(debug-emp-interpreter PUBLIC ${LIBMATA})
target_link_libraries(mata-emp-interpreter PUBLIC ${LIBMATA})
target_link_libraries(vata-emp-interpreter PUBLIC ${LIBMATA} libvata)
target_link_libraries(awali-emp-interpreter PUBLIC ${LIBMATA} ${LIBAWALI})
target_link_libraries(mona-emp-interpreter PUBLIC ${LIBMATA} ${LIBMONABDD} ${LIBMONADFA} ${LIBMONAMEM})