add_executable(nfa-emp-interpreter
                    interpreter_debug.cc)

target_link_libraries(nfa-emp-interpreter PUBLIC ${LIBMATA})