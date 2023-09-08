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
#include <mata/nfa/builder.hh>
#include <mata/parser/inter-aut.hh>

#include "interpreter/parser.h"
#include "interpreter/interpreter.h"
#include "utils/util.h"



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

    Instance<mata::nfa::Nfa> mataInst;
    mataInst.mata_to_nfa = [](const mata::IntermediateAut& t) -> mata::nfa::Nfa {
        mata::OnTheFlyAlphabet alphabet; // TODO: what to do with the alphabet
        TIME_BEGIN(construction);
        mata::nfa::Nfa aut = mata::nfa::builder::construct(t, &alphabet);
        TIME_END(construction);
        return aut;
    };
    mataInst.intersection = [](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        TIME_BEGIN(intersection);
        mata::nfa::Nfa aut = mata::nfa::intersection(a1, a2);
        TIME_END(intersection);
        return aut;
    };
    mataInst.is_empty = [](const mata::nfa::Nfa& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = mata::nfa::is_lang_empty(a1);
        TIME_END(emptiness_check);
        return empty;
    };

    std::ifstream input(program);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << program << std::endl;
        return -1;
    }

    try {
        Interpreter<mata::nfa::Nfa> interpret(mataInst, automata);
        interpret.run_program(input); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}