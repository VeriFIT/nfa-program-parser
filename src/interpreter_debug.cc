#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <exception>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <stdexcept>

#include <mata/nfa/nfa.hh>
#include <mata/parser/inter-aut.hh>
#include <mata/parser/mintermization.hh>

#include "parser.h"
#include "interpreter.h"



int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "error: Program expects exactly one argument, try '--help' for help" << std::endl;
        return -1;
    }

    std::string program = std::string(argv[1]);
    std::vector<std::string> automata;
    for(size_t i = 2; i < argc; i++) {
        automata.push_back(std::string(argv[i]));
    }

    Instance<std::string> dbgInst;
    dbgInst.mata_to_nfa = [](const mata::IntermediateAut& t) -> std::string {
        std::cout << "load(aut)" << std::endl;
        return "aut";
    };
    dbgInst.intersection = [](const std::string& a1, const std::string& a2) -> std::string {
        std::string res = "intersection (" + a1 + ", " + a2 + ")";
        std::cout << res << std::endl;
        return res;
    };
    dbgInst.is_empty = [](const std::string& a1) -> bool {
        std::cout << "is_empty (" + a1 + ")" << std::endl;
        return true;
    };

    std::ifstream input(program);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << program << std::endl;
        return -1;
    }

    try {
        Interpreter<std::string> interpret(dbgInst, automata);
        interpret.run_program(input); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}