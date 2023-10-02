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

#include <vata/explicit_tree_aut.hh>
#include <vata/parsing/timbuk_parser.hh>

#include "interpreter/parser.h"
#include "interpreter/interpreter.h"
#include "utils/util.h"

VATA::ExplicitTreeAut mata_to_vata(const mata::nfa::Nfa &nfa, const std::string &header, VATA::Parsing::TimbukParser &parser) {
    std::stringstream ss;
    ss << header;
    ss << "States";
    for (mata::nfa::State s = 0; s < nfa.num_of_states(); ++s) {
        ss << " q" << s;
    }
    ss << std::endl << "Final States";
    for (mata::nfa::State s : nfa.initial) {
        ss << " q" << s;
    }
    ss << std::endl << "Transitions" << std::endl;
    for (mata::nfa::State s : nfa.final) {
        ss << "x -> q" << s << std::endl;
    }
    for (const auto &tran : nfa.delta.transitions()) {
        ss << "a" << tran.symbol << "(q" << tran.target << ") -> q" << tran.source << std::endl;
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

    std::cout << std::boolalpha; 

    std::string program = std::string(argv[1]);
    std::vector<std::string> automata;
    for(size_t i = 2; i < argc; i++) {
        automata.push_back(std::string(argv[i]));
    }

    Instance<VATA::ExplicitTreeAut> vataInst;
    vataInst.mata_to_nfa = [](const mata::nfa::Nfa& t, const std::string& filename) -> VATA::ExplicitTreeAut {
        VATA::Parsing::TimbukParser parser;
        std::stringstream ss;
        ss << "Ops";
        for (mata::Symbol s : t.alphabet->get_alphabet_symbols()) {
            ss << " a" << s << ":1";
        }
        ss << " x:0" << std::endl << std::endl << "Automaton A" << std::endl;
        
        TIME_BEGIN(construction);
        VATA::ExplicitTreeAut aut = mata_to_vata(t, ss.str(), parser);
        TIME_END(construction);
        return aut;
    };
    vataInst.intersection = [](const VATA::ExplicitTreeAut& a1, const VATA::ExplicitTreeAut& a2) -> VATA::ExplicitTreeAut {
        TIME_BEGIN(intersection);
        VATA::ExplicitTreeAut aut = VATA::ExplicitTreeAut::Intersection(a1, a2);
        TIME_END(intersection);
        TIME_BEGIN(trim);
        aut = aut.RemoveUselessStates();
        TIME_END(trim);
        return aut;
    };
    vataInst.inter_all = [](const std::vector<VATA::ExplicitTreeAut>& auts) -> VATA::ExplicitTreeAut {
        assert(auts.size() > 0);
        VATA::ExplicitTreeAut tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            TIME_BEGIN(intersection);
            tmp = VATA::ExplicitTreeAut::Intersection(tmp, auts[i]);
            TIME_END(intersection);
            TIME_BEGIN(trim);
            tmp = tmp.RemoveUselessStates();
            TIME_END(trim);
        }
        return tmp;
    };
    vataInst.uni = [](const VATA::ExplicitTreeAut& a1, const VATA::ExplicitTreeAut& a2) -> VATA::ExplicitTreeAut {
        TIME_BEGIN(uni);
        VATA::ExplicitTreeAut aut = VATA::ExplicitTreeAut::Union(a1, a2);
        TIME_END(uni);
        TIME_BEGIN(trim);
        aut = aut.RemoveUselessStates();
        TIME_END(trim);
        return aut;
    };
    vataInst.uni_all = [](const std::vector<VATA::ExplicitTreeAut>& auts) -> VATA::ExplicitTreeAut {
        assert(auts.size() > 0);
        VATA::ExplicitTreeAut tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            TIME_BEGIN(uni);
            tmp = VATA::ExplicitTreeAut::Union(tmp, auts[i]);
            TIME_END(uni);
            TIME_BEGIN(trim);
            tmp = tmp.RemoveUselessStates();
            TIME_END(trim);
        }
        return tmp;
    };
    vataInst.concat = [](const VATA::ExplicitTreeAut& a1, const VATA::ExplicitTreeAut& a2) -> VATA::ExplicitTreeAut {
        throw std::runtime_error("concat is not implemented");
    };
    vataInst.complement = [](const VATA::ExplicitTreeAut& a1) -> VATA::ExplicitTreeAut {
        TIME_BEGIN(compl);
        VATA::ExplicitTreeAut aut = a1.Complement();
        TIME_END(compl);
        TIME_BEGIN(trim);
        aut = aut.RemoveUselessStates();
        TIME_END(trim);
        return aut;
    };
    vataInst.is_empty = [](const VATA::ExplicitTreeAut& a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = a1.IsLangEmpty();
        TIME_END(emptiness_check);
        std::cout << "emptiness_check_result: " << empty << std::endl;
        return empty;
    };
    vataInst.is_included = [](const VATA::ExplicitTreeAut& a1, const VATA::ExplicitTreeAut& a2) -> bool {
        VATA::InclParam inclParams;
        inclParams.SetAlgorithm(VATA::InclParam::e_algorithm::antichains);
        inclParams.SetDirection(VATA::InclParam::e_direction::upward);
        TIME_BEGIN(inclusion_check);
        bool incl = VATA::ExplicitTreeAut::CheckInclusion(a1, a2, inclParams);
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
        Interpreter<VATA::ExplicitTreeAut> interpret(vataInst, automata);
        TIME_BEGIN(overall);
        interpret.run_program(input);
        TIME_END(overall); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}
