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

extern "C" {
#include <mona/bdd.h>
#include <mona/dfa.h>
#include <mona/mem.h>
}

#include "interpreter/parser.h"
#include "interpreter/interpreter.h"
#include "utils/util.h"
#include "mona_mata.h"

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

    Instance<DFA*> monaInst;
    monaInst.mata_to_nfa = [](const mata::nfa::Nfa& t, const std::string& filename) -> DFA* {
       DFA* res = mona_input(filename);
       return res;
    };
    monaInst.intersection = [](DFA* a1, DFA* a2) -> DFA* {
        TIME_BEGIN(intersection);
        DFA* res = MonaDFA_product(a1, a2, dfaAND);
        TIME_END(intersection);
        return res;
    };
    monaInst.inter_all = [](const std::vector<DFA*>& auts) -> DFA* {
        assert(auts.size() > 0);
        DFA* tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            TIME_BEGIN(intersection);
            tmp = MonaDFA_product(tmp, auts[i], dfaAND);
            TIME_END(intersection);
        }
        return tmp;
    };
    monaInst.uni = [](DFA* a1, DFA* a2) -> DFA* {
        TIME_BEGIN(uni);
        DFA* res = MonaDFA_product(a1, a2, dfaOR);
        TIME_END(uni);
        return res;
    };
    monaInst.uni_all = [](const std::vector<DFA*>& auts) -> DFA* {
        assert(auts.size() > 0);
        DFA* tmp = auts[0];
        for(size_t i = 1; i < auts.size(); i++) {
            TIME_BEGIN(uni);
            tmp = MonaDFA_product(tmp, auts[i], dfaOR);
            TIME_END(uni);
        }
        return tmp;
    };
    monaInst.concat = [](DFA* a1, DFA* a2) -> DFA* {
        throw std::runtime_error("concat is not implemented");
        return a1;
    };
    monaInst.complement = [](DFA* a1) -> DFA* {
        TIME_BEGIN(compl);
        DFA *aut=dfaCopy(a1);
		dfaNegation(aut);
        TIME_END(compl);
        return aut;
    };
    monaInst.is_included = [](DFA* a1, DFA* a2) -> bool {
        TIME_BEGIN(inclusion_check);
        DFA *a2_compl=dfaCopy(a2);
        dfaNegation(a2_compl);
        bool incl = MonaDFA_check_empty( MonaDFA_product(a1, a2_compl,dfaAND));
        TIME_END(inclusion_check);
        std::cout << "inclusion_check_result: " << incl << std::endl;
        return incl;
    };
    monaInst.is_empty = [](DFA* a1) -> bool {
        TIME_BEGIN(emptiness_check);
        bool empty = MonaDFA_check_empty(a1);
        TIME_END(emptiness_check);
        std::cout << "emptiness_check_result: " << empty << std::endl;
        return empty;
    };

    std::ifstream input(program);
    if (!input.is_open()) {
        std::cerr << "error: could not open file " << program << std::endl;
        return -1;
    }

    try {
        Interpreter<DFA*> interpret(monaInst, automata);
        interpret.run_program(input); 
    } catch (const std::exception &exc) {
        std::cerr << "error: " << exc.what();
        return -1;
    }
}