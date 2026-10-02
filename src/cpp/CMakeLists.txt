add_executable(debug-emp-interpreter
    interpreter_debug.cc)
add_executable(mata-emp-interpreter
    interpreter_mata.cc)
# add_executable(vata-emp-interpreter
#     interpreter_vata.cc)
# add_executable(awali-emp-interpreter
#     interpreter_awali.cc)

target_include_directories(debug-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})
target_include_directories(mata-emp-interpreter PRIVATE ${MATA_INCLUDE_DIR})

target_link_libraries(debug-emp-interpreter PUBLIC ${LIBMATA})
target_link_libraries(mata-emp-interpreter PUBLIC ${LIBMATA})
# target_link_libraries(vata-emp-interpreter PUBLIC ${LIBMATA} libvata)
# target_link_libraries(awali-emp-interpreter PUBLIC ${LIBMATA} ${LIBAWALI})
