// A small reduced ordered BDD package, enough to turn the formulas of
// bits_load.hh into the guards of transitions: the BDD whose structure an
// interpreter copies into a library that keeps BDDs in a form of its own
// (mata's NFTs, MONA's DFAs), so that every such library starts from the same
// canonical guards.
#ifndef ROBDD_HH
#define ROBDD_HH

#include <algorithm>
#include <climits>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bits_load.hh"

namespace bench {

class Robdd {
public:
  using Node = std::uint32_t;
  // Not TRUE and FALSE, which MONA's bdd.h defines as macros.
  static constexpr Node ZERO{0};
  static constexpr Node ONE{1};
  /// The variable of the terminals, below every variable.
  static constexpr unsigned TERMINAL{UINT_MAX};

  Robdd() : nodes_{{TERMINAL, ZERO, ZERO}, {TERMINAL, ONE, ONE}} {}

  unsigned var(Node node) const { return nodes_[node].var; }
  Node low(Node node) const { return nodes_[node].low; }
  Node high(Node node) const { return nodes_[node].high; }

  Node make(unsigned var, Node low, Node high) {
    if (low == high) {
      return low;
    }
    const auto [it, inserted]{unique_.try_emplace(
        key(var, low, high), static_cast<Node>(nodes_.size()))};
    if (inserted) {
      nodes_.push_back({var, low, high});
    }
    return it->second;
  }

  Node negate(Node node) {
    if (node <= ONE) {
      return node ^ ONE;
    }
    if (const auto it{negations_.find(node)}; it != negations_.end()) {
      return it->second;
    }
    const Record rec{nodes_[node]};
    const Node result{make(rec.var, negate(rec.low), negate(rec.high))};
    negations_.emplace(node, result);
    return result;
  }

  Node conjoin(Node lhs, Node rhs) { return apply(lhs, rhs, true); }
  Node disjoin(Node lhs, Node rhs) { return apply(lhs, rhs, false); }

  /// The BDD of the formula rooted at @p formula of @p aut.
  Node of(const bits::Automaton &aut, unsigned formula) {
    const bits::Formula &node{aut.formulas[formula]};
    switch (node.kind) {
    case bits::Formula::Kind::True:
      return ONE;
    case bits::Formula::Kind::False:
      return ZERO;
    case bits::Formula::Kind::Var:
      return make(node.var, ZERO, ONE);
    case bits::Formula::Kind::Not:
      return negate(of(aut, node.lhs));
    case bits::Formula::Kind::And:
      return conjoin(of(aut, node.lhs), of(aut, node.rhs));
    case bits::Formula::Kind::Or:
      return disjoin(of(aut, node.lhs), of(aut, node.rhs));
    }
    return ZERO;
  }

  /// A guard: the disjunction of the formulas of the transitions from
  /// @c source to @c target.
  struct Guard {
    unsigned source;
    unsigned target;
    Node guard;
  };

  /// The satisfiable guards of @p aut, sorted by source and target.
  std::vector<Guard> guards(const bits::Automaton &aut) {
    std::unordered_map<std::uint64_t, Node> by_pair{};
    for (const bits::Transition &trans : aut.transitions) {
      const Node formula{of(aut, trans.formula)};
      const auto [it, inserted]{by_pair.try_emplace(
          (std::uint64_t{trans.source} << 32) | trans.target, formula)};
      if (!inserted) {
        it->second = disjoin(it->second, formula);
      }
    }
    std::vector<Guard> result{};
    for (const auto &[pair, guard] : by_pair) {
      if (guard != ZERO) {
        result.push_back({static_cast<unsigned>(pair >> 32),
                          static_cast<unsigned>(pair), guard});
      }
    }
    std::ranges::sort(result, {}, [](const Guard &guard) {
      return std::pair{guard.source, guard.target};
    });
    return result;
  }

private:
  struct Record {
    unsigned var;
    Node low;
    Node high;
  };

  static std::uint64_t key(unsigned var, Node low, Node high) {
    // At most 2^21 variables and nodes are far more than any guard needs.
    return (std::uint64_t{var} << 42) | (std::uint64_t{low} << 21) | high;
  }

  Node apply(Node lhs, Node rhs, bool conjunction) {
    if (conjunction) {
      if (lhs == ZERO || rhs == ZERO) {
        return ZERO;
      }
      if (lhs == ONE) {
        return rhs;
      }
      if (rhs == ONE || lhs == rhs) {
        return lhs;
      }
    } else {
      if (lhs == ONE || rhs == ONE) {
        return ONE;
      }
      if (lhs == ZERO) {
        return rhs;
      }
      if (rhs == ZERO || lhs == rhs) {
        return lhs;
      }
    }
    if (lhs > rhs) {
      std::swap(lhs, rhs);
    }
    auto &cache{conjunction ? conjunctions_ : disjunctions_};
    const std::uint64_t pair{(std::uint64_t{lhs} << 32) | rhs};
    if (const auto it{cache.find(pair)}; it != cache.end()) {
      return it->second;
    }
    const Record l{nodes_[lhs]};
    const Record r{nodes_[rhs]};
    const unsigned top{std::min(l.var, r.var)};
    const Node low{apply(l.var == top ? l.low : lhs, r.var == top ? r.low : rhs,
                         conjunction)};
    const Node high{apply(l.var == top ? l.high : lhs,
                          r.var == top ? r.high : rhs, conjunction)};
    const Node result{make(top, low, high)};
    cache.emplace(pair, result);
    return result;
  }

  std::vector<Record> nodes_;
  std::unordered_map<std::uint64_t, Node> unique_{};
  std::unordered_map<Node, Node> negations_{};
  std::unordered_map<std::uint64_t, Node> conjunctions_{};
  std::unordered_map<std::uint64_t, Node> disjunctions_{};
};

} // namespace bench

#endif
