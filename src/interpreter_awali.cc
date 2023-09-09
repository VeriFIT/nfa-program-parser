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

#include <awali/dyn.hh>

#include "interpreter/parser.h"
#include "interpreter/interpreter.h"
#include "utils/util.h"

using namespace awali::dyn;
using namespace awali;


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "error: Program expects at least one argument, try '--help' for help" << std::endl;
        return -1;
    }

    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        std::cout << "arguments: input.emp [aut.mata]*" << std::endl;
        return 0;
    }

    std::string program = std::string(argv[1]);
    std::vector<std::string> automata;
    for(size_t i = 2; i < argc; i++) {
        automata.push_back(std::string(argv[i]));
    }

    Instance<automaton_t> awaliInst;
    awaliInst.mata_to_nfa = [](const mata::IntermediateAut& t) -> automaton_t {
        mata::OnTheFlyAlphabet alphabet;
        mata::nfa::Nfa mata_aut = mata::nfa::builder::construct(t, &alphabet);
        std::map<mata::nfa::State, state_t> state_map {}; 

        std::string awali_alphabet;
        
        for (mata::Symbol s : alphabet.get_alphabet_symbols()) {
            if(s > 255) {
                throw std::out_of_range("awali symbol out of range");
            }
            awali_alphabet.push_back(s);
        }
        automaton_t awali_aut = automaton_t::from(awali_alphabet);
        
        TIME_BEGIN(construction);
        for (mata::nfa::State s = 0; s < mata_aut.size(); ++s) {
            state_t new_state = awali_aut->add_state();
            state_map[s] = new_state;
        }
        for (const auto &tran : mata_aut.delta.transitions()) {
            awali_aut->add_transition(state_map.at(tran.source), state_map.at(tran.target), char(tran.symbol));
        }
        for (mata::nfa::State s : mata_aut.final) {
            awali_aut->set_final(state_map.at(s));
        }
        for (mata::nfa::State s : mata_aut.initial) {
            awali_aut->set_initial(state_map.at(s));
        }
        TIME_END(construction);
        return awali_aut;
    };
    awaliInst.intersection = [](const automaton_t& a1, const automaton_t& a2) -> automaton_t {
        TIME_BEGIN(intersection);
        automaton_t res = product(a1, a2);
        TIME_END(intersection);
        return res;
    };
    awaliInst.uni = [](const automaton_t& a1, const automaton_t& a2) -> automaton_t {
        TIME_BEGIN(uni);
        automaton_t res = sum(a1, a2);
        TIME_END(uni);
        return res;
    };
    awaliInst.is_empty = [](const automaton_t& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = is_empty(trim(a1));
        TIME_END(emptiness_check);
        return empty;
    };
    awaliInst.is_included = [](const automaton_t& a1, const automaton_t& a2) -> bool {
        // TODO: it seems that awali does not support inclusion checking (just equivalence checking)
        throw std::runtime_error("awali does not support inclusion check");
    };

    std::ifstream input(program);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << program << std::endl;
        return -1;
    }

    try {
        Interpreter<automaton_t> interpret(awaliInst, automata);
        interpret.run_program(input); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}