#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <mata/nfa/nfa.hh>
#include <mata/parser/inter-aut.hh>

#include <awali/dyn.hh>

#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "utils/bench.hh"

using namespace awali::dyn;
using namespace awali;

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr
        << "error: Program expects at least one argument, try '--help' for help"
        << std::endl;
    return -1;
  }

  if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
    std::cout << "arguments: input.emp [aut.mata]*" << std::endl;
    return 0;
  }

  std::cout << std::boolalpha;

  std::string program = std::string(argv[1]);
  std::vector<std::string> automata;
  for (size_t i = 2; i < argc; i++) {
    automata.push_back(std::string(argv[i]));
  }

  Instance<automaton_t> awaliInst;
  awaliInst.mata_to_nfa = [](const mata::nfa::Nfa &t,
                             const std::string &filename) -> automaton_t {
    std::map<mata::nfa::State, state_t> state_map{};
    std::string awali_alphabet;
    for (mata::Symbol s : t.alphabet->get_alphabet_symbols()) {
      if (s > 255) {
        throw std::out_of_range("awali symbol out of range");
      }
      awali_alphabet.push_back(s);
    }
    automaton_t awali_aut = automaton_t::from(awali_alphabet);

    bench::measure("construction", [&] {
      for (mata::nfa::State s = 0; s < t.num_of_states(); ++s) {
        state_t new_state = awali_aut->add_state();
        state_map[s] = new_state;
      }
      for (const auto &tran : t.delta.transitions()) {
        awali_aut->add_transition(state_map.at(tran.source),
                                  state_map.at(tran.target), char(tran.symbol));
      }
      for (mata::nfa::State s : t.final) {
        awali_aut->set_final(state_map.at(s));
      }
      for (mata::nfa::State s : t.initial) {
        awali_aut->set_initial(state_map.at(s));
      }
    });
    return awali_aut;
  };
  awaliInst.intersection = [](const automaton_t &a1,
                              const automaton_t &a2) -> automaton_t {
    automaton_t res =
        bench::measure("intersection", [&] { return product(a1, a2); });
    bench::measure("trim", [&] { res = trim(res); });
    return res;
  };
  awaliInst.inter_all =
      [](const std::vector<automaton_t> &auts) -> automaton_t {
    assert(auts.size() > 0);
    automaton_t tmp = auts[0];
    for (size_t i = 1; i < auts.size(); i++) {
      bench::measure("intersection", [&] { tmp = product(tmp, auts[i]); });
      bench::measure("trim", [&] { tmp = trim(tmp); });
    }
    return tmp;
  };
  awaliInst.uni = [](const automaton_t &a1,
                     const automaton_t &a2) -> automaton_t {
    automaton_t res = bench::measure("uni", [&] { return sum(a1, a2); });
    bench::measure("trim", [&] { res = trim(res); });
    return res;
  };
  awaliInst.uni_all = [](const std::vector<automaton_t> &auts) -> automaton_t {
    assert(auts.size() > 0);
    automaton_t tmp = auts[0];
    for (size_t i = 1; i < auts.size(); i++) {
      bench::measure("uni", [&] { tmp = sum(tmp, auts[i]); });
      bench::measure("trim", [&] { tmp = trim(tmp); });
    }
    return tmp;
  };
  awaliInst.concat = [](const automaton_t &a1,
                        const automaton_t &a2) -> automaton_t {
    automaton_t res = bench::measure(
        "concat", [&] { return concatenate(standard(a1), standard(a2)); });
    bench::measure("trim", [&] { res = trim(res); });
    return res;
  };
  awaliInst.complement = [](const automaton_t &a1) -> automaton_t {
    automaton_t aut = bench::measure(
        "compl", [&] { return complement(complete(determinize(a1))); });
    bench::measure("trim", [&] { aut = trim(aut); });
    return aut;
  };
  awaliInst.is_empty = [](const automaton_t &a1) -> bool {
    bool empty =
        bench::measure("emptiness_check", [&] { return is_empty(trim(a1)); });
    bench::report("result", empty);
    return empty;
  };
  awaliInst.determinize = [](const automaton_t &a1) -> automaton_t {
    automaton_t res =
        bench::measure("determinize", [&] { return determinize(a1); });
    return res;
  };
  awaliInst.minimize = [](const automaton_t &a1) -> automaton_t {
    const automaton_t min = bench::measure("minimize", [&] {
      return minimal_automaton(
          a1, {MINIM_ALGO = DETERMINIZE_QUOTIENT, QUOTIENT_ALGO = HOPCROFT});
    });
    return min;
  };

  awaliInst.is_included = [](const automaton_t &a1,
                             const automaton_t &a2) -> bool {
    // TODO: it seems that awali does not support inclusion checking (just
    // equivalence checking)
    bool incl = bench::measure("inclusion_check", [&] {
      return is_empty(trim(product(a1, complement(complete(determinize(a2))))));
    });
    bench::report("result", incl);
    return incl;
  };

  std::ifstream input(program);
  if (!input.is_open()) {
    std::cerr << "error: could not open file " << program << std::endl;
    return -1;
  }

  try {
    Interpreter<automaton_t> interpret(awaliInst, automata);
    bench::measure("overall", [&] { interpret.run_program(input); });
  } catch (const std::exception &exc) {
    std::cerr << "error: " << exc.what();
    return -1;
  }
}
