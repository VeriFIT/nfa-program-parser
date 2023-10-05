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

#include "interpreter/parser.h"
#include "interpreter/interpreter.h"
#include "utils/util.h"
#include "utils/common.h"

#define SIM_THRESHOLD 1000

#define REDUCE_WRAP(aut) aut.num_of_states() <= SIM_THRESHOLD ? mata::nfa::reduce(aut) : aut

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "error: Program expects at least one argument, try '--help' for help" << std::endl;
        return -1;
    }

    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        std::cout << "arguments: input.emp [arguments]* [aut.mata]*" << std::endl;
        return 0;
    }

    // pool of alphabets
    std::vector<mata::OnTheFlyAlphabet> alphabets {};
    std::cout << std::boolalpha; 
    // input parameters
    Params params = parse_input_args(argc, argv);

    Instance<mata::nfa::Nfa> mataInst;
    mataInst.mata_to_nfa = [&alphabets](const mata::nfa::Nfa& t, const std::string& filename) -> mata::nfa::Nfa {
        return t;
    };
    mataInst.intersection = [&params](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        mata::nfa::Nfa a1prime = a1;
        mata::nfa::Nfa a2prime = a2;
        if(params.sim_red) {
            TIME_BEGIN(reduce);
            a1prime = REDUCE_WRAP(a1);
            a2prime = REDUCE_WRAP(a2);
            TIME_END(reduce);
        }
        
        TIME_BEGIN(intersection);
        mata::nfa::Nfa aut = mata::nfa::intersection(a1prime, a2prime);
        TIME_END(intersection);
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        aut.alphabet = a1.alphabet;
        return aut;
    };
    mataInst.inter_all = [&params](const std::vector<mata::nfa::Nfa>& auts) -> mata::nfa::Nfa {
        assert(auts.size() > 0);
        mata::nfa::Nfa tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            if(params.sim_red) {
                TIME_BEGIN(reduce);
                tmp = REDUCE_WRAP(tmp);
                TIME_END(reduce);
            }
            TIME_BEGIN(intersection);
            tmp = mata::nfa::intersection(tmp, auts[i]);
            TIME_END(intersection);
            TIME_BEGIN(trim);
            tmp.trim();
            TIME_END(trim);
        }
        tmp.alphabet = auts[0].alphabet;
        return tmp;
    };
    mataInst.uni = [&params](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        mata::nfa::Nfa aut = a1;
        TIME_BEGIN(uni);
        aut.uni(a2);
        TIME_END(uni);
        aut.alphabet = a1.alphabet;
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        return aut;
    };
    mataInst.uni_all = [&params](const std::vector<mata::nfa::Nfa>& auts) -> mata::nfa::Nfa {
        assert(auts.size() > 0);
        mata::nfa::Nfa tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            TIME_BEGIN(uni);
            tmp.uni(auts[i]);
            TIME_END(uni);
            TIME_BEGIN(trim);
            tmp.trim();
            TIME_END(trim);
        }
        tmp.alphabet = auts[0].alphabet;
        return tmp;
    };
    mataInst.concat = [&params](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        mata::nfa::Nfa aut = a1;
        if(params.sim_red) {
            TIME_BEGIN(reduce);
            aut = REDUCE_WRAP(aut);
            TIME_END(reduce);
        }
        TIME_BEGIN(concat);
        aut.concatenate(a2);
        TIME_END(concat);
        aut.alphabet = a1.alphabet;
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        return aut;
    };
    mataInst.complement = [&params](const mata::nfa::Nfa& a1) -> mata::nfa::Nfa {
        mata::nfa::Nfa a1prime = a1;
        if(params.sim_red) {
            TIME_BEGIN(reduce);
            a1prime = REDUCE_WRAP(a1);
            TIME_END(reduce);
        }
        TIME_BEGIN(compl);
        mata::nfa::Nfa aut = mata::nfa::complement(a1prime, *a1.alphabet, 
            {{"algorithm", "classical"}, {"minimize", "false"}});
        TIME_END(compl);
        aut.alphabet = a1.alphabet;
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        return aut;
    };
    mataInst.is_empty = [&params](const mata::nfa::Nfa& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = a1.is_lang_empty();
        TIME_END(emptiness_check);
        std::cout << "emptiness_check_result: " << empty << std::endl;
        return empty;
    };
    mataInst.is_included = [&params](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> bool {
        mata::nfa::Nfa a1prime = a1;
        mata::nfa::Nfa a2prime = a2;
        if(params.sim_red) {
            TIME_BEGIN(reduce);
            a1prime = REDUCE_WRAP(a1);
            a2prime = REDUCE_WRAP(a2);
            TIME_END(reduce);
        }
        TIME_BEGIN(inclusion_check);
        bool incl = mata::nfa::is_included(a1prime, a2prime);
        TIME_END(inclusion_check);
        std::cout << "inclusion_check_result: " << incl << std::endl;
        return incl;
    };

    std::ifstream input(params.program);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << params.program << std::endl;
        return -1;
    }

    try {
        Interpreter<mata::nfa::Nfa> interpret(mataInst, params.automata);
        TIME_BEGIN(overall);
        interpret.run_program(input);
        TIME_END(overall); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}
