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

#include <mata/applications/strings.hh>
#include <mata/nfa/algorithms.hh>
#include <mata/nfa/nfa.hh>
#include <mata/nft/nft.hh>
#include <mata/parser/inter-aut.hh>

#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "utils/bench.hh"
#include "utils/common.h"

#define SIM_THRESHOLD 1000

#define REDUCE_WRAP(aut)                                                       \
  aut.num_of_states() <= SIM_THRESHOLD ? mata::nfa::reduce(aut) : aut

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr
        << "error: Program expects at least one argument, try '--help' for help"
        << std::endl;
    return -1;
  }

  if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
    std::cout << "arguments: input.emp [arguments]* [aut.mata]*" << std::endl;
    return 0;
  }

  // pool of alphabets
  std::vector<mata::OnTheFlyAlphabet> alphabets{};
  std::cout << std::boolalpha;
  // input parameters
  Params params = parse_input_args(argc, argv);

  Instance<mata::nfa::Nfa> mataInst;
  mataInst.mata_to_nfa =
      [&alphabets](const mata::nfa::Nfa &t,
                   const std::string &filename) -> mata::nfa::Nfa { return t; };
  mataInst.intersection =
      [&params](const mata::nfa::Nfa &a1,
                const mata::nfa::Nfa &a2) -> mata::nfa::Nfa {
    mata::nfa::Nfa a1prime = a1;
    mata::nfa::Nfa a2prime = a2;
    if (params.sim_red) {
      bench::measure("reduce", [&] {
        a1prime = REDUCE_WRAP(a1);
        a2prime = REDUCE_WRAP(a2);
      });
    }
    mata::nfa::Nfa aut{bench::measure("intersection", [&] {
      return mata::nfa::intersection(a1prime, a2prime);
    })};
    bench::measure("trim", [&] { aut.trim(); });
    aut.alphabet = a1.alphabet;
    return aut;
  };
  mataInst.inter_all =
      [&params](const std::vector<mata::nfa::Nfa> &auts) -> mata::nfa::Nfa {
    assert(auts.size() > 0);
    mata::nfa::Nfa tmp = auts[0];
    for (size_t i = 1; i < auts.size(); i++) {
      if (params.sim_red) {
        bench::measure("reduce", [&] { tmp = REDUCE_WRAP(tmp); });
      }
      tmp = bench::measure("intersection", [&] {
        return mata::nfa::intersection(tmp, auts[i]);
      });
      bench::measure("trim", [&] { tmp.trim(); });
    }
    tmp.alphabet = auts[0].alphabet;
    return tmp;
  };
  mataInst.uni = [&params](const mata::nfa::Nfa &a1,
                           const mata::nfa::Nfa &a2) -> mata::nfa::Nfa {
    mata::nfa::Nfa aut = a1;
    bench::measure("uni", [&] { aut.unite_nondet_with(a2); });
    aut.alphabet = a1.alphabet;
    bench::measure("trim", [&] { aut.trim(); });
    return aut;
  };
  mataInst.uni_all =
      [&params](const std::vector<mata::nfa::Nfa> &auts) -> mata::nfa::Nfa {
    assert(auts.size() > 0);
    mata::nfa::Nfa tmp = auts[0];
    for (size_t i = 1; i < auts.size(); i++) {
      bench::measure("uni", [&] { tmp.unite_nondet_with(auts[i]); });
      bench::measure("trim", [&] { tmp.trim(); });
    }
    tmp.alphabet = auts[0].alphabet;
    return tmp;
  };
  mataInst.concat = [&params](const mata::nfa::Nfa &a1,
                              const mata::nfa::Nfa &a2) -> mata::nfa::Nfa {
    mata::nfa::Nfa aut = a1;
    if (params.sim_red) {
      bench::measure("reduce", [&] { aut = REDUCE_WRAP(aut); });
    }
    bench::measure("concat", [&] { aut.concatenate(a2); });
    aut.alphabet = a1.alphabet;
    bench::measure("trim", [&] { aut.trim(); });
    return aut;
  };
  mataInst.complement = [&params](const mata::nfa::Nfa &a1) -> mata::nfa::Nfa {
    mata::nfa::Nfa a1prime = a1;
    if (params.sim_red) {
      bench::measure("reduce", [&] { a1prime = REDUCE_WRAP(a1); });
    }
    mata::nfa::Nfa aut{bench::measure("compl", [&] {
      return mata::nfa::complement(
          a1prime, *a1.alphabet,
          {{"algorithm", "classical"}, {"minimize", "false"}});
    })};
    aut.alphabet = a1.alphabet;
    bench::measure("trim", [&] { aut.trim(); });
    return aut;
  };
  mataInst.is_empty = [&params](const mata::nfa::Nfa &a1) -> bool {
    const bool empty{
        bench::measure("emptiness_check", [&] { return a1.is_lang_empty(); })};
    bench::report("emptiness_check_result", empty);
    return empty;
  };
  mataInst.determinize = [&params](const mata::nfa::Nfa &a1) -> mata::nfa::Nfa {
    mata::nfa::Nfa res{bench::measure(
        "determinize", [&] { return mata::nfa::determinize(a1); })};
    res.alphabet = a1.alphabet;
    return res;
  };
  mataInst.minimize = [&params](const mata::nfa::Nfa &a1) -> mata::nfa::Nfa {
    mata::nfa::Nfa res{bench::measure("minimize", [&] {
      return mata::nfa::algorithms::minimize_hopcroft(a1);
    })};
    res.alphabet = a1.alphabet;
    return res;
  };

  mataInst.is_included = [&params](const mata::nfa::Nfa &a1,
                                   const mata::nfa::Nfa &a2) -> bool {
    mata::nfa::Nfa a1prime = a1;
    mata::nfa::Nfa a2prime = a2;
    if (params.sim_red) {
      bench::measure("reduce", [&] {
        a1prime = REDUCE_WRAP(a1);
        a2prime = REDUCE_WRAP(a2);
      });
    }
    const bool incl{bench::measure("inclusion_check", [&] {
      return mata::nfa::is_included(a1prime, a2prime);
    })};
    bench::report("inclusion_check_result", incl);
    return incl;
  };

  mataInst.words_of_lengths = [](const mata::nfa::Nfa &a1,
                                 const std::vector<unsigned> &lengths) -> bool {
    // An NFA is a one-tape transducer; the lengths of further tapes can only be
    // satisfied by an NFT, which the '.mata' loader does not produce.
    const mata::nft::Nft nft{a1, lengths.size()};
    const std::optional<std::vector<mata::Word>> words{
        bench::measure("words_of_lengths", [&] {
          return mata::applications::strings::get_words_of_lengths(nft,
                                                                   lengths);
        })};
    bench::report("result", words.has_value() ? "sat" : "unsat");
    return words.has_value();
  };

  std::ifstream input(params.program);

  if (!input.is_open()) {
    std::cerr << "error: could not open file " << params.program << std::endl;
    return -1;
  }

  try {
    Interpreter<mata::nfa::Nfa> interpret(mataInst, params.automata);
    bench::measure("overall", [&] { interpret.run_program(input); });
  } catch (const std::exception &exc) {
    std::cerr << "error: " << exc.what();
    return -1;
  }
}
