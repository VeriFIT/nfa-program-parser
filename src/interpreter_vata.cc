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

#include <vata/explicit_tree_aut.hh>
#include <vata/parsing/timbuk_parser.hh>

#include "interpreter/parser.h"
#include "interpreter/interpreter.h"
#include "utils/util.h"

VATA::ExplicitTreeAut mata_to_vata(const mata::nfa::Nfa &nfa, const std::string &header, VATA::Parsing::TimbukParser &parser) {
    std::stringstream ss;
    ss << header;
    ss << "States";
    for (mata::nfa::State s = 0; s < nfa.size(); ++s) {
        ss << " q" << s;
    }
    ss << std::endl << "Final States";
    for (mata::nfa::State s : nfa.final) {
        ss << " q" << s;
    }
    ss << std::endl << "Transitions" << std::endl;
    for (mata::nfa::State s : nfa.initial) {
        ss << "x -> q" << s << std::endl;
    }
    for (const auto &tran : nfa.delta.transitions()) {
        ss << "a" << tran.symbol << "(q" << tran.source << ") -> q" << tran.target << std::endl;
    }
    VATA::ExplicitTreeAut result;
    result.LoadFromString(parser, ss.str());
    return result;
}

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

    Instance<VATA::ExplicitTreeAut> vataInst;
    vataInst.mata_to_nfa = [](const mata::IntermediateAut& t) -> VATA::ExplicitTreeAut {
        mata::OnTheFlyAlphabet alphabet;
        VATA::Parsing::TimbukParser parser;

        std::stringstream ss;
        ss << "Ops";
        for (mata::Symbol s : alphabet.get_alphabet_symbols()) {
            ss << " a" << s << ":1";
        }
        ss << " x:0" << std::endl << std::endl << "Automaton A" << std::endl;
        
        mata::nfa::Nfa mata_aut = mata::nfa::builder::construct(t, &alphabet);
        TIME_BEGIN(construction);
        VATA::ExplicitTreeAut aut = mata_to_vata(mata_aut, ss.str(), parser);
        TIME_END(construction);
        return aut;
    };
    vataInst.intersection = [](const VATA::ExplicitTreeAut& a1, const VATA::ExplicitTreeAut& a2) -> VATA::ExplicitTreeAut {
        TIME_BEGIN(intersection);
        VATA::ExplicitTreeAut aut = VATA::ExplicitTreeAut::Intersection(a1, a2);
        TIME_END(intersection);
        return aut;
    };
    vataInst.is_empty = [](const VATA::ExplicitTreeAut& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = a1.IsLangEmpty();
        TIME_END(emptiness_check);
        return empty;
    };

    std::ifstream input(program);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << program << std::endl;
        return -1;
    }

    try {
        Interpreter<VATA::ExplicitTreeAut> interpret(vataInst, automata);
        interpret.run_program(input); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}