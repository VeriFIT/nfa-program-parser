add_executable(nfa-emp-interpreter
                    interpreter.cc)

target_link_libraries(nfa-emp-interpreter PUBLIC ${LIBMATA})