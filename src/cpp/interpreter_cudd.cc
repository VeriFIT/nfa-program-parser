// The interpreter on symbolic NFAs whose transitions are labelled by CUDD BDDs.
//
// An automaton over bitvectors keeps explicit states and, from each state, one
// BDD guard per target: the set of bitvectors it moves on. The operations are
// the textbook ones over the guards: the product conjoins them, the subset
// construction splits the guards of a macrostate into the regions of its
// targets (complement completes it to the region no guard covers), and
// inclusion is the antichain algorithm over the product of the smaller
// automaton with the subset construction of the bigger one.
#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cudd.h>

#include "interpreter/interpreter.h"
#include "interpreter/parser.h"
#include "utils/bench.hh"
#include "utils/bits_load.hh"

namespace {

DdManager *manager{nullptr};

/// A referenced BDD of the manager.
class Bdd {
public:
  Bdd() = default;
  /// Takes a result of CUDD, which is not referenced yet.
  explicit Bdd(DdNode *node) : node_{node} {
    if (node_ == nullptr) {
      throw std::runtime_error("CUDD ran out of memory");
    }
    Cudd_Ref(node_);
  }
  Bdd(const Bdd &other) : node_{other.node_} {
    if (node_ != nullptr) {
      Cudd_Ref(node_);
    }
  }
  Bdd(Bdd &&other) noexcept : node_{std::exchange(other.node_, nullptr)} {}
  Bdd &operator=(Bdd other) noexcept {
    std::swap(node_, other.node_);
    return *this;
  }
  ~Bdd() {
    if (node_ != nullptr) {
      Cudd_RecursiveDeref(manager, node_);
    }
  }

  static Bdd one() { return Bdd{Cudd_ReadOne(manager)}; }
  static Bdd zero() { return Bdd{Cudd_ReadLogicZero(manager)}; }
  static Bdd var(unsigned index) {
    return Bdd{Cudd_bddIthVar(manager, static_cast<int>(index))};
  }

  Bdd operator&(const Bdd &rhs) const {
    return Bdd{Cudd_bddAnd(manager, node_, rhs.node_)};
  }
  Bdd operator|(const Bdd &rhs) const {
    return Bdd{Cudd_bddOr(manager, node_, rhs.node_)};
  }
  Bdd operator!() const { return Bdd{Cudd_Not(node_)}; }
  bool is_zero() const { return node_ == Cudd_ReadLogicZero(manager); }

private:
  DdNode *node_{nullptr};
};

using State = unsigned;
using StateSet = std::vector<State>; // sorted

struct SymNfa {
  /// From each state, its targets in increasing order with satisfiable guards.
  std::vector<std::vector<std::pair<State, Bdd>>> post{};
  std::vector<State> initial{};
  std::vector<char> final{};

  State add_state(bool is_final = false) {
    post.emplace_back();
    final.push_back(is_final ? 1 : 0);
    return static_cast<State>(post.size() - 1);
  }
  size_t num_of_states() const { return post.size(); }
};

Bdd formula_bdd(const bench::bits::Automaton &aut, unsigned formula) {
  const bench::bits::Formula &node{aut.formulas[formula]};
  switch (node.kind) {
  case bench::bits::Formula::Kind::True:
    return Bdd::one();
  case bench::bits::Formula::Kind::False:
    return Bdd::zero();
  case bench::bits::Formula::Kind::Var:
    return Bdd::var(node.var);
  case bench::bits::Formula::Kind::Not:
    return !formula_bdd(aut, node.lhs);
  case bench::bits::Formula::Kind::And:
    return formula_bdd(aut, node.lhs) & formula_bdd(aut, node.rhs);
  case bench::bits::Formula::Kind::Or:
    return formula_bdd(aut, node.lhs) | formula_bdd(aut, node.rhs);
  }
  throw std::logic_error("unknown formula kind");
}

SymNfa to_sym_nfa(const bench::bits::Automaton &aut) {
  SymNfa nfa{};
  for (unsigned state{0}; state < aut.num_of_states; ++state) {
    nfa.add_state();
  }
  nfa.initial = aut.initial;
  for (const unsigned state : aut.final) {
    nfa.final[state] = 1;
  }
  std::vector<std::map<State, Bdd>> guards(aut.num_of_states);
  for (const bench::bits::Transition &trans : aut.transitions) {
    const Bdd formula{formula_bdd(aut, trans.formula)};
    const auto [it, inserted]{
        guards[trans.source].try_emplace(trans.target, formula)};
    if (!inserted) {
      it->second = it->second | formula;
    }
  }
  for (State state{0}; state < aut.num_of_states; ++state) {
    for (auto &[target, guard] : guards[state]) {
      if (!guard.is_zero()) {
        nfa.post[state].emplace_back(target, std::move(guard));
      }
    }
  }
  std::ranges::sort(nfa.initial);
  nfa.initial.erase(std::ranges::unique(nfa.initial).begin(),
                    nfa.initial.end());
  return nfa;
}

/// The states reachable from @p from by @p succ.
template <typename Successors>
std::vector<char> reach(size_t num_of_states, const std::vector<State> &from,
                        const Successors &succ) {
  std::vector<char> reached(num_of_states, 0);
  std::vector<State> stack{};
  for (const State state : from) {
    if (!reached[state]) {
      reached[state] = 1;
      stack.push_back(state);
    }
  }
  while (!stack.empty()) {
    const State state{stack.back()};
    stack.pop_back();
    succ(state, [&](State next) {
      if (!reached[next]) {
        reached[next] = 1;
        stack.push_back(next);
      }
    });
  }
  return reached;
}

/// Keeps the states reachable from an initial state and reaching a final one.
SymNfa trim(const SymNfa &nfa) {
  const size_t n{nfa.num_of_states()};
  const std::vector<char> forward{
      reach(n, nfa.initial, [&](State state, const auto &visit) {
        for (const auto &[target, guard] : nfa.post[state]) {
          visit(target);
        }
      })};
  std::vector<std::vector<State>> pre(n);
  std::vector<State> finals{};
  for (State state{0}; state < n; ++state) {
    for (const auto &[target, guard] : nfa.post[state]) {
      pre[target].push_back(state);
    }
    if (nfa.final[state]) {
      finals.push_back(state);
    }
  }
  const std::vector<char> backward{
      reach(n, finals, [&](State state, const auto &visit) {
        for (const State source : pre[state]) {
          visit(source);
        }
      })};
  std::vector<State> renaming(n, UINT32_MAX);
  SymNfa result{};
  for (State state{0}; state < n; ++state) {
    if (forward[state] && backward[state]) {
      renaming[state] = result.add_state(nfa.final[state]);
    }
  }
  for (State state{0}; state < n; ++state) {
    if (renaming[state] == UINT32_MAX) {
      continue;
    }
    for (const auto &[target, guard] : nfa.post[state]) {
      if (renaming[target] != UINT32_MAX) {
        result.post[renaming[state]].emplace_back(renaming[target], guard);
      }
    }
  }
  for (const State state : nfa.initial) {
    if (renaming[state] != UINT32_MAX) {
      result.initial.push_back(renaming[state]);
    }
  }
  return result;
}

SymNfa intersection(const SymNfa &lhs, const SymNfa &rhs) {
  SymNfa result{};
  std::unordered_map<std::uint64_t, State> pairs{};
  std::vector<std::pair<State, State>> worklist{};
  const auto product = [&](State l, State r) {
    const auto [it,
                inserted]{pairs.try_emplace((std::uint64_t{l} << 32) | r, 0)};
    if (inserted) {
      it->second = result.add_state(lhs.final[l] && rhs.final[r]);
      worklist.emplace_back(l, r);
    }
    return it->second;
  };
  for (const State l : lhs.initial) {
    for (const State r : rhs.initial) {
      result.initial.push_back(product(l, r));
    }
  }
  while (!worklist.empty()) {
    const auto [l, r]{worklist.back()};
    worklist.pop_back();
    const State source{pairs.at((std::uint64_t{l} << 32) | r)};
    for (const auto &[l_target, l_guard] : lhs.post[l]) {
      for (const auto &[r_target, r_guard] : rhs.post[r]) {
        Bdd guard{l_guard & r_guard};
        if (!guard.is_zero()) {
          const State target{product(l_target, r_target)};
          result.post[source].emplace_back(target, std::move(guard));
        }
      }
    }
  }
  for (auto &post : result.post) {
    std::ranges::sort(post, {}, &std::pair<State, Bdd>::first);
  }
  return result;
}

SymNfa union_nondet(const SymNfa &lhs, const SymNfa &rhs) {
  SymNfa result{lhs};
  const auto offset{static_cast<State>(lhs.num_of_states())};
  for (State state{0}; state < rhs.num_of_states(); ++state) {
    const State copy{result.add_state(rhs.final[state])};
    for (const auto &[target, guard] : rhs.post[state]) {
      result.post[copy].emplace_back(target + offset, guard);
    }
  }
  for (const State state : rhs.initial) {
    result.initial.push_back(state + offset);
  }
  return result;
}

/// The moves of a macrostate: for each target, the union of the guards to it.
std::vector<std::pair<State, Bdd>> moves_of(const SymNfa &nfa,
                                            const StateSet &states) {
  std::map<State, Bdd> moves{};
  for (const State state : states) {
    for (const auto &[target, guard] : nfa.post[state]) {
      const auto [it, inserted]{moves.try_emplace(target, guard)};
      if (!inserted) {
        it->second = it->second | guard;
      }
    }
  }
  return {std::make_move_iterator(moves.begin()),
          std::make_move_iterator(moves.end())};
}

/// Splits @p domain into the regions moving to the same set of targets of
/// @p moves; the regions are disjoint and non-empty, their target sets are
/// sorted and pairwise distinct.
std::vector<std::pair<Bdd, StateSet>>
split(Bdd domain, const std::vector<std::pair<State, Bdd>> &moves) {
  std::vector<std::pair<Bdd, StateSet>> regions{};
  regions.emplace_back(std::move(domain), StateSet{});
  std::vector<std::pair<Bdd, StateSet>> refined{};
  for (const auto &[target, guard] : moves) {
    refined.clear();
    for (auto &[region, targets] : regions) {
      Bdd inside{region & guard};
      if (inside.is_zero()) {
        refined.emplace_back(std::move(region), std::move(targets));
        continue;
      }
      Bdd outside{region & !guard};
      if (!outside.is_zero()) {
        refined.emplace_back(std::move(outside), targets);
      }
      targets.push_back(target);
      refined.emplace_back(std::move(inside), std::move(targets));
    }
    std::swap(regions, refined);
  }
  return regions;
}

/// The subset construction; with @p complete, also the sink, the empty
/// macrostate, of the bitvectors no move reads.
SymNfa determinize(const SymNfa &nfa, bool complete) {
  SymNfa result{};
  std::map<StateSet, State> macrostates{};
  std::vector<const StateSet *> worklist{};
  const auto macrostate = [&](StateSet states) {
    const auto [it, inserted]{macrostates.try_emplace(std::move(states), 0)};
    if (inserted) {
      it->second = result.add_state(std::ranges::any_of(
          it->first, [&](State state) { return nfa.final[state] != 0; }));
      worklist.push_back(&it->first);
    }
    return it->second;
  };
  result.initial.push_back(macrostate(nfa.initial));
  while (!worklist.empty()) {
    const StateSet &states{*worklist.back()};
    worklist.pop_back();
    const State source{macrostates.at(states)};
    const std::vector<std::pair<State, Bdd>> moves{moves_of(nfa, states)};
    for (auto &[region, targets] : split(Bdd::one(), moves)) {
      if (!targets.empty() || complete) {
        const State target{macrostate(std::move(targets))};
        result.post[source].emplace_back(target, std::move(region));
      }
    }
  }
  for (auto &post : result.post) {
    std::ranges::sort(post, {}, &std::pair<State, Bdd>::first);
  }
  return result;
}

bool is_empty(const SymNfa &nfa) {
  const std::vector<char> reached{reach(
      nfa.num_of_states(), nfa.initial, [&](State state, const auto &visit) {
        for (const auto &[target, guard] : nfa.post[state]) {
          visit(target);
        }
      })};
  for (State state{0}; state < nfa.num_of_states(); ++state) {
    if (reached[state] && nfa.final[state]) {
      return false;
    }
  }
  return true;
}

/// Antichain inclusion: explores the pairs of a state of @p smaller and a
/// macrostate of @p bigger, keeping per state only the minimal macrostates.
bool is_included(const SymNfa &smaller, const SymNfa &bigger) {
  std::map<StateSet, unsigned> ids{};
  std::vector<const StateSet *> sets{};
  std::vector<char> accepting{};
  std::vector<std::vector<std::pair<State, Bdd>>> moves{};
  const auto intern = [&](StateSet states) {
    const auto [it, inserted]{
        ids.try_emplace(std::move(states), static_cast<unsigned>(sets.size()))};
    if (inserted) {
      sets.push_back(&it->first);
      accepting.push_back(std::ranges::any_of(
          it->first, [&](State state) { return bigger.final[state] != 0; }));
      moves.push_back(moves_of(bigger, it->first));
    }
    return it->second;
  };
  std::vector<std::vector<unsigned>> antichain(smaller.num_of_states());
  std::vector<std::pair<State, unsigned>> worklist{};
  // Adds the pair unless a pair with a subset of its macrostate is known;
  // false when the pair is a counterexample.
  const auto visit = [&](State state, unsigned id) {
    if (smaller.final[state] && !accepting[id]) {
      return false;
    }
    std::vector<unsigned> &known{antichain[state]};
    for (const unsigned other : known) {
      if (std::ranges::includes(*sets[id], *sets[other])) {
        return true;
      }
    }
    std::erase_if(known, [&](unsigned other) {
      return std::ranges::includes(*sets[other], *sets[id]);
    });
    known.push_back(id);
    worklist.emplace_back(state, id);
    return true;
  };
  const unsigned initial{intern(bigger.initial)};
  for (const State state : smaller.initial) {
    if (!visit(state, initial)) {
      return false;
    }
  }
  while (!worklist.empty()) {
    const auto [state, id]{worklist.back()};
    worklist.pop_back();
    if (!std::ranges::contains(antichain[state], id)) {
      continue; // Subsumed since it was added.
    }
    for (const auto &[target, guard] : smaller.post[state]) {
      for (auto &[region, targets] : split(guard, moves[id])) {
        if (!visit(target, intern(std::move(targets)))) {
          return false;
        }
      }
    }
  }
  return true;
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

  // Dynamic variable reordering is off by default: the variables keep the
  // order of the inputs, as in the other interpreters.
  manager = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);

  Instance<SymNfa> inst;
  inst.load = [](const std::vector<std::string> &files) {
    const bench::bits::Inputs inputs{bench::bits::load(files)};
    return bench::measure("construction", [&] {
      std::vector<SymNfa> nfas{};
      for (const bench::bits::Automaton &aut : inputs.automata) {
        nfas.push_back(to_sym_nfa(aut));
      }
      return nfas;
    });
  };
  const auto intersect = [](const SymNfa &lhs, const SymNfa &rhs) {
    const SymNfa product{
        bench::measure("intersection", [&] { return intersection(lhs, rhs); })};
    return bench::measure("trim", [&] { return trim(product); });
  };
  const auto unite = [](const SymNfa &lhs, const SymNfa &rhs) {
    const SymNfa united{
        bench::measure("uni", [&] { return union_nondet(lhs, rhs); })};
    return bench::measure("trim", [&] { return trim(united); });
  };
  inst.intersection = intersect;
  inst.inter_all = [intersect](const std::vector<SymNfa> &nfas) {
    SymNfa result{nfas.at(0)};
    for (size_t i{1}; i < nfas.size(); ++i) {
      result = intersect(result, nfas[i]);
    }
    return result;
  };
  inst.uni = unite;
  inst.uni_all = [unite](const std::vector<SymNfa> &nfas) {
    SymNfa result{nfas.at(0)};
    for (size_t i{1}; i < nfas.size(); ++i) {
      result = unite(result, nfas[i]);
    }
    return result;
  };
  inst.complement = [](const SymNfa &nfa) {
    SymNfa result{bench::measure("compl", [&] {
      SymNfa dfa{determinize(nfa, true)};
      for (char &is_final : dfa.final) {
        is_final = !is_final;
      }
      return dfa;
    })};
    return bench::measure("trim", [&] { return trim(result); });
  };
  inst.determinize = [](const SymNfa &nfa) {
    return bench::measure("determinize",
                          [&] { return determinize(nfa, false); });
  };
  inst.is_empty = [](const SymNfa &nfa) {
    const bool empty{
        bench::measure("emptiness_check", [&] { return is_empty(nfa); })};
    bench::report("result", empty);
    return empty;
  };
  inst.is_included = [](const SymNfa &smaller, const SymNfa &bigger) {
    const bool included{bench::measure(
        "inclusion_check", [&] { return is_included(smaller, bigger); })};
    bench::report("result", included);
    return included;
  };

  std::ifstream input(program);
  if (!input.is_open()) {
    std::cerr << "error: could not open file " << program << std::endl;
    return -1;
  }
  try {
    Interpreter<SymNfa> interpret(inst, automata);
    bench::measure("overall", [&] { interpret.run_program(input); });
  } catch (const std::exception &exc) {
    std::cerr << "error: " << exc.what();
    return -1;
  }
}
