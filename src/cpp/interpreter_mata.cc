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



int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "error: Program expects at least one argument, try '--help' for help" << std::endl;
        return -1;
    }

    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        std::cout << "arguments: input.emp [aut.mata]*" << std::endl;
        return 0;
    }

    // pool of alphabets
    std::vector<mata::OnTheFlyAlphabet> alphabets {};
    std::cout << std::boolalpha; 

    std::string program = std::string(argv[1]);
    std::vector<std::string> automata;
    for(size_t i = 2; i < argc; i++) {
        automata.push_back(std::string(argv[i]));
    }

    Instance<mata::nfa::Nfa> mataInst;
    mataInst.mata_to_nfa = [&alphabets](const mata::nfa::Nfa& t, const std::string& filename) -> mata::nfa::Nfa {
        return t;
    };
    mataInst.intersection = [](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        TIME_BEGIN(intersection);
        mata::nfa::Nfa aut = mata::nfa::intersection(a1, a2);
        TIME_END(intersection);
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        aut.alphabet = a1.alphabet;
        return aut;
    };
    mataInst.inter_all = [](const std::vector<mata::nfa::Nfa>& auts) -> mata::nfa::Nfa {
        assert(auts.size() > 0);
        mata::nfa::Nfa tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
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
    mataInst.uni = [](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        TIME_BEGIN(uni);
        mata::nfa::Nfa aut = mata::nfa::uni(a1, a2);
        TIME_END(uni);
        aut.alphabet = a1.alphabet;
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        return aut;
    };
    mataInst.uni_all = [](const std::vector<mata::nfa::Nfa>& auts) -> mata::nfa::Nfa {
        assert(auts.size() > 0);
        mata::nfa::Nfa tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            TIME_BEGIN(uni);
            tmp = mata::nfa::uni(tmp, auts[i]);
            TIME_END(uni);
            TIME_BEGIN(trim);
            tmp.trim();
            TIME_END(trim);
        }
        tmp.alphabet = auts[0].alphabet;
        return tmp;
    };
    mataInst.concat = [](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> mata::nfa::Nfa {
        mata::nfa::Nfa aut = a1;
        TIME_BEGIN(concat);
        aut.concatenate(a2);
        TIME_END(concat);
        aut.alphabet = a1.alphabet;
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        return aut;
    };
    mataInst.complement = [](const mata::nfa::Nfa& a1) -> mata::nfa::Nfa {
        TIME_BEGIN(compl);
        mata::nfa::Nfa aut = mata::nfa::complement(a1, *a1.alphabet, 
            {{"algorithm", "classical"}, {"minimize", "false"}});
        TIME_END(compl);
        aut.alphabet = a1.alphabet;
        TIME_BEGIN(trim);
        aut.trim();
        TIME_END(trim);
        return aut;
    };
    mataInst.is_empty = [](const mata::nfa::Nfa& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = a1.is_lang_empty();
        TIME_END(emptiness_check);
        std::cout << "emptiness_check_result: " << empty << std::endl;
        return empty;
    };
    mataInst.is_included = [](const mata::nfa::Nfa& a1, const mata::nfa::Nfa& a2) -> bool {
        TIME_BEGIN(inclusion_check);
        bool incl = mata::nfa::is_included(a1, a2);
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
        Interpreter<mata::nfa::Nfa> interpret(mataInst, automata);
        interpret.run_program(input); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}
