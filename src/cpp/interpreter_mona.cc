// The interpreter on MONA's DFA library: deterministic automata whose states
// each have one shared multi-terminal BDD over the bitvector, mapping it to the
// target.
//
// MONA has no nondeterministic automata. An NFA is loaded the way MONA
// quantifies existentially: each state gets choice variables, above the
// bitvector's, numbering its targets, so that the automaton is deterministic
// over the bitvector and the choices; projecting the choices out
// (dfaProject, then dfaMinimize, as MONA does) is the subset construction. This
// determinization is part of the construction, as every operation of MONA
// works on DFAs: the product (then dfaMinimize) for intersection, union and
// inclusion, and swapping the accepting states for complement.
#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <mona/bdd.h>
#include <mona/dfa.h>
#include <mona/mem.h>
}

#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "utils/bench.hh"
#include "utils/bits_load.hh"
#include "utils/robdd.hh"

namespace {

using Dfa = std::shared_ptr<DFA>;

Dfa own(DFA *dfa) { return Dfa{dfa, dfaFree}; }

Dfa minimize(const Dfa &dfa) {
  return own(
      bench::measure("minimize", [&] { return dfaMinimize(dfa.get()); }));
}

/// Builds the DFA of an NFA: the choice variables of a state's targets are the
/// indices 0, ..., choices - 1, the bitvector's variables follow.
class Loader {
public:
  Loader(bench::Robdd &bdd, unsigned choices) : bdd_{bdd}, choices_{choices} {}

  Dfa load(const bench::bits::Automaton &aut) {
    // The guards of every state, by target; the initial states are merged into
    // a fresh one unless there is exactly one.
    std::vector<std::map<unsigned, bench::Robdd::Node>> moves(
        aut.num_of_states);
    for (const bench::Robdd::Guard &guard : bdd_.guards(aut)) {
      moves[guard.source].emplace(guard.target, guard.guard);
    }
    std::vector<char> accepting(aut.num_of_states, 0);
    for (const unsigned state : aut.final) {
      accepting[state] = 1;
    }
    std::vector<unsigned> initial{aut.initial};
    std::ranges::sort(initial);
    initial.erase(std::ranges::unique(initial).begin(), initial.end());
    unsigned start{0};
    if (initial.size() == 1) {
      start = initial.front();
    } else {
      start = static_cast<unsigned>(moves.size());
      std::map<unsigned, bench::Robdd::Node> merged{};
      char merged_accepting{0};
      for (const unsigned state : initial) {
        for (const auto &[target, guard] : moves[state]) {
          const auto [it, inserted]{merged.emplace(target, guard)};
          if (!inserted) {
            it->second = bdd_.disjoin(it->second, guard);
          }
        }
        merged_accepting |= accepting[state];
      }
      moves.push_back(std::move(merged));
      accepting.push_back(merged_accepting);
    }
    // An empty set of initial states starts in the sink.
    const auto sink{static_cast<unsigned>(moves.size())};
    if (initial.empty()) {
      start = sink;
    }
    const auto num_of_states{sink + 1};

    DFA *dfa{dfaMake(static_cast<int>(num_of_states))};
    dfa->s = static_cast<int>(start);
    bddm_ = dfa->bddm;
    leaves_.clear();
    nodes_.clear();
    for (unsigned state{0}; state < sink; ++state) {
      dfa->f[state] = accepting[state] ? 1 : -1;
      const std::vector<std::pair<unsigned, bench::Robdd::Node>> targets(
          moves[state].begin(), moves[state].end());
      dfa->q[state] = choice(targets, 0, 0, sink);
    }
    dfa->f[sink] = -1;
    dfa->q[sink] = leaf(sink);

    Dfa result{own(dfa)};
    // The choices of a target are the variables 0, ..., choices_ - 1.
    for (unsigned var{0}; var < choices_; ++var) {
      result = own(bench::measure(
          "determinize", [&] { return dfaProject(result.get(), var); }));
      result = minimize(result);
    }
    if (choices_ == 0) {
      result = minimize(result);
    }
    return result;
  }

private:
  bench::Robdd &bdd_;
  unsigned choices_;
  bdd_manager *bddm_{nullptr};
  std::unordered_map<unsigned, bdd_ptr> leaves_{};
  std::unordered_map<std::uint64_t, bdd_ptr> nodes_{};

  bdd_ptr leaf(unsigned state) {
    const auto [it, inserted]{leaves_.try_emplace(state, 0)};
    if (inserted) {
      it->second = bdd_find_leaf_sequential(bddm_, state);
    }
    return it->second;
  }

  bdd_ptr node(unsigned index, bdd_ptr low, bdd_ptr high) {
    return low == high ? low
                       : bdd_find_node_sequential(bddm_, low, high, index);
  }

  /// The choice variables from @p var down, @p code their bits above.
  bdd_ptr
  choice(const std::vector<std::pair<unsigned, bench::Robdd::Node>> &targets,
         unsigned var, std::size_t code, unsigned sink) {
    if (var == choices_) {
      return code < targets.size()
                 ? guard(targets[code].second, targets[code].first, sink)
                 : leaf(sink);
    }
    const std::size_t bit{std::size_t{1} << (choices_ - 1 - var)};
    return node(var, choice(targets, var + 1, code, sink),
                choice(targets, var + 1, code | bit, sink));
  }

  /// The guard @p node leading to @p target, and to the sink elsewhere.
  bdd_ptr guard(bench::Robdd::Node node_of_guard, unsigned target,
                unsigned sink) {
    if (node_of_guard == bench::Robdd::ONE) {
      return leaf(target);
    }
    if (node_of_guard == bench::Robdd::ZERO) {
      return leaf(sink);
    }
    const std::uint64_t key{(std::uint64_t{node_of_guard} << 32) | target};
    if (const auto it{nodes_.find(key)}; it != nodes_.end()) {
      return it->second;
    }
    const bdd_ptr low{guard(bdd_.low(node_of_guard), target, sink)};
    const bdd_ptr high{guard(bdd_.high(node_of_guard), target, sink)};
    const bdd_ptr result{node(choices_ + bdd_.var(node_of_guard), low, high)};
    nodes_.emplace(key, result);
    return result;
  }
};

std::vector<unsigned> *successors{nullptr};
void collect_successor(unsigned state) { successors->push_back(state); }

bool is_empty(const Dfa &dfa) {
  std::vector<char> reached(static_cast<std::size_t>(dfa->ns), 0);
  std::vector<unsigned> stack{static_cast<unsigned>(dfa->s)};
  reached[dfa->s] = 1;
  std::vector<unsigned> next{};
  successors = &next;
  while (!stack.empty()) {
    const unsigned state{stack.back()};
    stack.pop_back();
    if (dfa->f[state] == 1) {
      return false;
    }
    next.clear();
    bdd_call_leafs(dfa->bddm, dfa->q[state], collect_successor);
    for (const unsigned target : next) {
      if (!reached[target]) {
        reached[target] = 1;
        stack.push_back(target);
      }
    }
  }
  return true;
}

Dfa product(const Dfa &lhs, const Dfa &rhs, int mode, const char *name) {
  const Dfa result{own(bench::measure(name, [&] {
    return dfaProduct(lhs.get(), rhs.get(), static_cast<dfaProductType>(mode));
  }))};
  return minimize(result);
}

/// dfaProduct's mode: bit 2 * lhs + rhs of the statuses is the result's.
constexpr int LHS_AND_NOT_RHS{4};

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

  bdd_init();

  Instance<Dfa> inst;
  inst.load = [](const std::vector<std::string> &files) {
    const bench::bits::Inputs inputs{bench::bits::load(files)};
    return bench::measure("construction", [&] {
      bench::Robdd bdd{};
      // One number of choices for all, enough for the most targets of a state,
      // so that a variable of the bitvector has one index in every automaton.
      std::size_t most_targets{1};
      for (const bench::bits::Automaton &aut : inputs.automata) {
        std::vector<std::size_t> degrees(aut.num_of_states + 1, 0);
        for (const bench::Robdd::Guard &guard : bdd.guards(aut)) {
          ++degrees[guard.source];
        }
        // A fresh initial state takes the targets of every initial one.
        for (const unsigned state : aut.initial) {
          degrees.back() += degrees[state];
        }
        most_targets = std::max(most_targets, std::ranges::max(degrees));
      }
      unsigned choices{0};
      while ((std::size_t{1} << choices) < most_targets) {
        ++choices;
      }
      Loader loader{bdd, choices};
      std::vector<Dfa> dfas{};
      for (const bench::bits::Automaton &aut : inputs.automata) {
        dfas.push_back(loader.load(aut));
      }
      return dfas;
    });
  };
  const auto intersect = [](const Dfa &lhs, const Dfa &rhs) {
    return product(lhs, rhs, dfaAND, "intersection");
  };
  const auto unite = [](const Dfa &lhs, const Dfa &rhs) {
    return product(lhs, rhs, dfaOR, "uni");
  };
  inst.intersection = intersect;
  inst.inter_all = [intersect](const std::vector<Dfa> &dfas) {
    Dfa result{dfas.at(0)};
    for (std::size_t i{1}; i < dfas.size(); ++i) {
      result = intersect(result, dfas[i]);
    }
    return result;
  };
  inst.uni = unite;
  inst.uni_all = [unite](const std::vector<Dfa> &dfas) {
    Dfa result{dfas.at(0)};
    for (std::size_t i{1}; i < dfas.size(); ++i) {
      result = unite(result, dfas[i]);
    }
    return result;
  };
  inst.complement = [](const Dfa &dfa) {
    return own(bench::measure("compl", [&] {
      DFA *result{dfaCopy(dfa.get())};
      dfaNegation(result);
      return result;
    }));
  };
  // Every automaton of MONA is deterministic.
  inst.determinize = [](const Dfa &dfa) { return dfa; };
  inst.minimize = [](const Dfa &dfa) { return minimize(dfa); };
  inst.is_empty = [](const Dfa &dfa) {
    const bool empty{
        bench::measure("emptiness_check", [&] { return is_empty(dfa); })};
    bench::report("result", empty);
    return empty;
  };
  inst.is_included = [](const Dfa &smaller, const Dfa &bigger) {
    const bool included{bench::measure("inclusion_check", [&] {
      const Dfa difference{
          own(dfaProduct(smaller.get(), bigger.get(),
                         static_cast<dfaProductType>(LHS_AND_NOT_RHS)))};
      return is_empty(difference);
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
    Interpreter<Dfa> interpret(inst, automata);
    bench::measure("overall", [&] { interpret.run_program(input); });
  } catch (const std::exception &exc) {
    std::cerr << "error: " << exc.what();
    return -1;
  }
}
