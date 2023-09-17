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
    
    std::cout << std::boolalpha; 

    std::string program = std::string(argv[1]);
    std::vector<std::string> automata;
    for(size_t i = 2; i < argc; i++) {
        automata.push_back(std::string(argv[i]));
    }

    Instance<automaton_t> awaliInst;
    awaliInst.mata_to_nfa = [](const mata::nfa::Nfa& t, const std::string& filename) -> automaton_t {
        std::map<mata::nfa::State, state_t> state_map {}; 
        std::string awali_alphabet;
        for (mata::Symbol s : t.alphabet->get_alphabet_symbols()) {
            if(s > 255) {
                throw std::out_of_range("awali symbol out of range");
            }
            awali_alphabet.push_back(s);
        }
        automaton_t awali_aut = automaton_t::from(awali_alphabet);
        
        TIME_BEGIN(construction);
        for (mata::nfa::State s = 0; s < t.size(); ++s) {
            state_t new_state = awali_aut->add_state();
            state_map[s] = new_state;
        }
        for (const auto &tran : t.delta.transitions()) {
            awali_aut->add_transition(state_map.at(tran.source), state_map.at(tran.target), char(tran.symbol));
        }
        for (mata::nfa::State s : t.final) {
            awali_aut->set_final(state_map.at(s));
        }
        for (mata::nfa::State s : t.initial) {
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
    awaliInst.inter_all = [](const std::vector<automaton_t>& auts) -> automaton_t {
        assert(auts.size() > 0);
        automaton_t tmp = auts[0];
        TIME_BEGIN(interall);
        for(size_t i = 1; i < auts.size(); i++) {
            tmp = product(tmp, auts[i]);
        }
        TIME_END(interall);
        return tmp;
    };
    awaliInst.uni = [](const automaton_t& a1, const automaton_t& a2) -> automaton_t {
        TIME_BEGIN(uni);
        automaton_t res = sum(a1, a2);
        TIME_END(uni);
        return res;
    };
    awaliInst.complement = [](const automaton_t& a1) -> automaton_t {
        TIME_BEGIN(compl);
        automaton_t aut = complement(complete(determinize(a1)));
        TIME_END(compl);
        return aut;
    };
    awaliInst.is_empty = [](const automaton_t& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = is_empty(trim(a1));
        TIME_END(emptiness_check);
        std::cout << "emptiness_check_result: " << empty << std::endl;
        return empty;
    };
    awaliInst.is_included = [](const automaton_t& a1, const automaton_t& a2) -> bool {
        // TODO: it seems that awali does not support inclusion checking (just equivalence checking)
        TIME_BEGIN(inclusion_check);
        bool incl = is_empty(trim(product(a1, complement(complete(determinize(a2))))));
        TIME_END(inclusion_check);
        std::cout << "inclusion_check_result: " << incl << std::endl;
        return incl;
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