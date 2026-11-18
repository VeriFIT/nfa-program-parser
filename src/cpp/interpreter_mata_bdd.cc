// The interpreter on mata's NFTs used as automata with BDDs on transitions.
//
// An automaton over bitvectors of n variables is an NFT of n levels: its states
// are the states of level 0, and the transitions from a state to a state are
// the BDD of their guard, its nodes the states of the levels of their
// variables, reading 0 or 1. A node whose child skips variables jumps over
// their levels with JumpMode::AppendDontCares, which reads DONT_CARE there, as
// a guard that does not depend on its first variables starts with a DONT_CARE
// jump. Every operation runs on this NFT with mata's NFT algorithms:
// intersection and inclusion read DONT_CAREs and jumps themselves; complement
// and determinization unwind them into 0 and 1 first (as 'unwind').
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <mata/alphabet.hh>
#include <mata/nft/nft.hh>

#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "utils/bench.hh"
#include "utils/bits_load.hh"
#include "utils/robdd.hh"

namespace {

using mata::Symbol;
using mata::nft::JumpMode;
using mata::nft::Level;
using mata::nft::Nft;
using mata::nft::State;

constexpr JumpMode JUMP_MODE{JumpMode::AppendDontCares};
const mata::utils::OrdVector<Symbol> BITS{0, 1};

/// The NFT of @p aut, whose guards are decided by @p num_of_levels variables.
Nft to_nft(bench::Robdd &bdd, const bench::bits::Automaton &aut,
           size_t num_of_levels) {
  Nft nft{Nft::with_levels(num_of_levels)};
  for (unsigned state{0}; state < aut.num_of_states; ++state) {
    nft.add_state_with_level(0);
  }
  for (const unsigned state : aut.initial) {
    nft.initial.insert(state);
  }
  for (const unsigned state : aut.final) {
    nft.final.insert(state);
  }

  // The state of a BDD node towards a target is shared by the guards to it.
  std::unordered_map<std::uint64_t, State> node_states{};
  struct Builder {
    bench::Robdd &bdd;
    Nft &nft;
    std::unordered_map<std::uint64_t, State> &node_states;

    State state_of(bench::Robdd::Node node, State target) {
      if (node == bench::Robdd::ONE) {
        return target;
      }
      const auto [it, inserted]{node_states.try_emplace(
          (std::uint64_t{node} << 32) | target, State{0})};
      if (!inserted) {
        return it->second;
      }
      const State state{
          nft.add_state_with_level(static_cast<Level>(bdd.var(node)))};
      it->second = state;
      add_children(state, node, target);
      return state;
    }

    /// The transitions of @p source reading the variable of @p node.
    void add_children(State source, bench::Robdd::Node node, State target) {
      if (const bench::Robdd::Node low{bdd.low(node)};
          low != bench::Robdd::ZERO) {
        nft.delta.add(source, 0, state_of(low, target));
      }
      if (const bench::Robdd::Node high{bdd.high(node)};
          high != bench::Robdd::ZERO) {
        nft.delta.add(source, 1, state_of(high, target));
      }
    }
  } builder{bdd, nft, node_states};

  for (const bench::Robdd::Guard &guard : bdd.guards(aut)) {
    if (guard.guard != bench::Robdd::ONE && bdd.var(guard.guard) == 0) {
      builder.add_children(guard.source, guard.guard, guard.target);
    } else {
      nft.delta.add(guard.source, mata::nft::DONT_CARE,
                    builder.state_of(guard.guard, guard.target));
    }
  }
  return nft;
}

/// The NFT of @p nft without jumps and DONT_CAREs, reading 0 or 1 on every
/// level, which complementation and determinization need.
Nft unwind(const Nft &nft) {
  return bench::measure("unwind",
                        [&] { return nft.unwind_jumps(BITS, JUMP_MODE); });
}

} // namespace

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
  const std::string program{argv[1]};
  const std::vector<std::string> automata(argv + 2, argv + argc);

  Instance<Nft> inst;
  inst.load = [](const std::vector<std::string> &files) {
    const bench::bits::Inputs inputs{bench::bits::load(files)};
    return bench::measure("construction", [&] {
      bench::Robdd bdd{};
      const size_t num_of_levels{std::max<size_t>(inputs.variables.size(), 1)};
      std::vector<Nft> nfts{};
      for (const bench::bits::Automaton &aut : inputs.automata) {
        nfts.push_back(to_nft(bdd, aut, num_of_levels));
      }
      return nfts;
    });
  };
  const auto intersect = [](const Nft &lhs, const Nft &rhs) {
    Nft result{bench::measure("intersection", [&] {
      return mata::nft::intersection(lhs, rhs, nullptr, JUMP_MODE);
    })};
    bench::measure("trim", [&] { result.trim(); });
    return result;
  };
  const auto unite = [](const Nft &lhs, const Nft &rhs) {
    Nft result{bench::measure(
        "uni", [&] { return mata::nft::union_nondet(lhs, rhs); })};
    bench::measure("trim", [&] { result.trim(); });
    return result;
  };
  inst.intersection = intersect;
  inst.inter_all = [intersect](const std::vector<Nft> &nfts) {
    Nft result{nfts.at(0)};
    for (size_t i{1}; i < nfts.size(); ++i) {
      result = intersect(result, nfts[i]);
    }
    return result;
  };
  inst.uni = unite;
  inst.uni_all = [unite](const std::vector<Nft> &nfts) {
    Nft result{nfts.at(0)};
    for (size_t i{1}; i < nfts.size(); ++i) {
      result = unite(result, nfts[i]);
    }
    return result;
  };
  inst.complement = [](const Nft &nft) {
    const Nft unwound{unwind(nft)};
    Nft result{bench::measure("compl", [&] {
      return mata::nft::complement(unwound, BITS, {{"algorithm", "classical"}});
    })};
    bench::measure("trim", [&] { result.trim(); });
    return result;
  };
  inst.determinize = [](const Nft &nft) {
    const Nft unwound{unwind(nft)};
    return bench::measure("determinize",
                          [&] { return mata::nft::determinize(unwound); });
  };
  inst.is_empty = [](const Nft &nft) {
    const bool empty{
        bench::measure("emptiness_check", [&] { return nft.is_lang_empty(); })};
    bench::report("result", empty);
    return empty;
  };
  inst.is_included = [](const Nft &smaller, const Nft &bigger) {
    const mata::EnumAlphabet alphabet{BITS};
    const bool included{bench::measure("inclusion_check", [&] {
      return mata::nft::is_included(smaller, bigger, &alphabet, JUMP_MODE);
    })};
    bench::report("result", included);
    return included;
  };

  std::ifstream input(program);
  if (!input.is_open()) {
    std::cerr << "error: could not open file " << program << std::endl;
    return -1;
  }
  try {
    Interpreter<Nft> interpret(inst, automata);
    bench::measure("overall", [&] { interpret.run_program(input); });
  } catch (const std::exception &exc) {
    std::cerr << "error: " << exc.what();
    return -1;
  }
}
