// Reading of '.mata' NFAs over bitvector alphabets ('@NFA-bits') with the
// formula of every transition kept, for the interpreters whose automata carry
// BDDs on their transitions and so must not mintermize. It does not use mata,
// so that every such library reads its inputs alike.
//
// A transition line is '<source> <formula> <target>', a formula is built from
// variables, '\true', '\false', '!', '&', '|' and parentheses ('!' binds
// tightest, '|' loosest), '%Initial' and '%Final' list states.
#ifndef BITS_LOAD_HH
#define BITS_LOAD_HH

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bench.hh"

namespace bench::bits {

/// A node of a Boolean formula over the variables of the bitvector.
struct Formula {
  enum class Kind : std::uint8_t { True, False, Var, Not, And, Or };
  Kind kind{Kind::True};
  /// Var: the index of the variable in Inputs::variables.
  unsigned var{0};
  /// Not: lhs; And and Or: lhs and rhs; indices into Automaton::formulas.
  unsigned lhs{0};
  unsigned rhs{0};
};

struct Transition {
  unsigned source;
  /// The root of the formula in Automaton::formulas.
  unsigned formula;
  unsigned target;
};

/// An NFA whose states are 0, ..., num_of_states - 1.
struct Automaton {
  unsigned num_of_states{0};
  std::vector<unsigned> initial{};
  std::vector<unsigned> final{};
  std::vector<Transition> transitions{};
  std::vector<Formula> formulas{};
};

/// The automata of one instance, over one set of variables.
struct Inputs {
  std::vector<Automaton> automata{};
  /// The variables in the order a BDD decides them, from its root: the
  /// variables named by a common prefix and a number (a15, ..., a0) go from
  /// the highest number down, as the formulas list them.
  std::vector<std::string> variables{};
};

namespace detail {

class FormulaParser {
public:
  FormulaParser(std::string_view text, std::vector<Formula> &pool,
                std::map<std::string, unsigned, std::less<>> &variables)
      : text_{text}, pool_{pool}, variables_{variables} {}

  unsigned parse() {
    const unsigned root{disjunction()};
    skip_spaces();
    if (pos_ != text_.size()) {
      fail("unexpected '" + std::string{text_.substr(pos_)} + "'");
    }
    return root;
  }

private:
  std::string_view text_;
  std::size_t pos_{0};
  std::vector<Formula> &pool_;
  std::map<std::string, unsigned, std::less<>> &variables_;

  [[noreturn]] void fail(const std::string &what) const {
    throw std::runtime_error("formula '" + std::string{text_} + "': " + what);
  }

  void skip_spaces() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  bool accept(char c) {
    skip_spaces();
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  unsigned add(Formula formula) {
    pool_.push_back(formula);
    return static_cast<unsigned>(pool_.size() - 1);
  }

  unsigned disjunction() {
    unsigned lhs{conjunction()};
    while (accept('|')) {
      lhs = add({Formula::Kind::Or, 0, lhs, conjunction()});
    }
    return lhs;
  }

  unsigned conjunction() {
    unsigned lhs{negation()};
    while (accept('&')) {
      lhs = add({Formula::Kind::And, 0, lhs, negation()});
    }
    return lhs;
  }

  unsigned negation() {
    if (accept('!')) {
      return add({Formula::Kind::Not, 0, negation(), 0});
    }
    if (accept('(')) {
      const unsigned inner{disjunction()};
      if (!accept(')')) {
        fail("missing ')'");
      }
      return inner;
    }
    skip_spaces();
    const std::size_t start{pos_};
    if (pos_ < text_.size() && text_[pos_] == '\\') {
      ++pos_;
    }
    while (pos_ < text_.size() &&
           (std::isalnum(static_cast<unsigned char>(text_[pos_])) ||
            text_[pos_] == '_')) {
      ++pos_;
    }
    const std::string_view name{text_.substr(start, pos_ - start)};
    if (name == "\\true") {
      return add({Formula::Kind::True});
    }
    if (name == "\\false") {
      return add({Formula::Kind::False});
    }
    if (name.empty() || name.front() == '\\') {
      fail("expected a variable at offset " + std::to_string(start));
    }
    auto it{variables_.find(name)};
    if (it == variables_.end()) {
      it = variables_.emplace(std::string{name}, variables_.size()).first;
    }
    return add({Formula::Kind::Var, it->second});
  }
};

inline std::vector<std::string_view> split(std::string_view line) {
  std::vector<std::string_view> tokens{};
  std::size_t pos{0};
  while (pos < line.size()) {
    while (pos < line.size() &&
           std::isspace(static_cast<unsigned char>(line[pos]))) {
      ++pos;
    }
    const std::size_t start{pos};
    while (pos < line.size() &&
           !std::isspace(static_cast<unsigned char>(line[pos]))) {
      ++pos;
    }
    if (pos > start) {
      tokens.push_back(line.substr(start, pos - start));
    }
  }
  return tokens;
}

/// Reads @p file, numbering the variables of its formulas in @p variables in
/// the order they appear in; the caller renumbers them.
inline Automaton
read_automaton(const std::string &file,
               std::map<std::string, unsigned, std::less<>> &variables) {
  std::ifstream input{file};
  if (!input) {
    throw std::runtime_error("could not open file: " + file);
  }
  Automaton aut{};
  std::unordered_map<std::string, unsigned> states{};
  const auto state = [&](std::string_view name) {
    const auto [it, inserted]{states.emplace(name, aut.num_of_states)};
    aut.num_of_states += inserted ? 1 : 0;
    return it->second;
  };
  bool header{false};
  std::string line{};
  while (std::getline(input, line)) {
    const std::vector<std::string_view> tokens{split(line)};
    if (tokens.empty() || tokens.front().starts_with('#')) {
      continue;
    }
    if (tokens.front().starts_with('@')) {
      if (header || tokens.front() != "@NFA-bits") {
        throw std::runtime_error(file +
                                 ": expected one '@NFA-bits' section, got '" +
                                 std::string{tokens.front()} + "'");
      }
      header = true;
    } else if (!header) {
      throw std::runtime_error(file + ": no '@NFA-bits' section header");
    } else if (tokens.front() == "%Initial" || tokens.front() == "%Final") {
      std::vector<unsigned> &states_of{
          tokens.front() == "%Initial" ? aut.initial : aut.final};
      for (std::size_t i{1}; i < tokens.size(); ++i) {
        states_of.push_back(state(tokens[i]));
      }
    } else if (tokens.front().starts_with('%')) {
      continue; // Namings and alphabet keys: states and variables are read
                // alike.
    } else if (tokens.size() < 3) {
      throw std::runtime_error(file + ": not a transition: '" + line + "'");
    } else {
      const std::size_t begin{
          static_cast<std::size_t>(tokens[1].data() - line.data())};
      const std::size_t end{
          static_cast<std::size_t>(tokens.back().data() - line.data())};
      const unsigned formula{
          FormulaParser{std::string_view{line}.substr(begin, end - begin),
                        aut.formulas, variables}
              .parse()};
      aut.transitions.push_back(
          {state(tokens.front()), formula, state(tokens.back())});
    }
  }
  if (!header) {
    throw std::runtime_error(file + ": no '@NFA-bits' section");
  }
  return aut;
}

/// The order of Inputs::variables: by the prefix of the name, then by the
/// number that ends it from the highest down, then by the name.
inline bool variable_before(const std::string &lhs, const std::string &rhs) {
  const auto split_number = [](const std::string &name) {
    std::size_t digits{name.size()};
    while (digits > 0 &&
           std::isdigit(static_cast<unsigned char>(name[digits - 1]))) {
      --digits;
    }
    const std::string_view number{std::string_view{name}.substr(digits)};
    return std::pair{std::string_view{name}.substr(0, digits), number};
  };
  const auto [lhs_prefix, lhs_number]{split_number(lhs)};
  const auto [rhs_prefix, rhs_number]{split_number(rhs)};
  if (lhs_prefix != rhs_prefix) {
    return lhs_prefix < rhs_prefix;
  }
  if (lhs_number.size() != rhs_number.size()) {
    return lhs_number.size() > rhs_number.size();
  }
  return lhs_number > rhs_number;
}

} // namespace detail

/// Reads @p files, the automata of one instance. Times 'parsing'.
inline Inputs load(const std::vector<std::string> &files) {
  return measure("parsing", [&] {
    Inputs inputs{};
    std::map<std::string, unsigned, std::less<>> variables{};
    for (const std::string &file : files) {
      inputs.automata.push_back(detail::read_automaton(file, variables));
    }
    std::vector<std::string> names(variables.size());
    for (const auto &[name, index] : variables) {
      names[index] = name;
    }
    inputs.variables = names;
    std::ranges::sort(inputs.variables, detail::variable_before);
    std::vector<unsigned> renaming(names.size());
    for (unsigned order{0}; order < inputs.variables.size(); ++order) {
      renaming[variables.at(inputs.variables[order])] = order;
    }
    for (Automaton &aut : inputs.automata) {
      for (Formula &formula : aut.formulas) {
        if (formula.kind == Formula::Kind::Var) {
          formula.var = renaming[formula.var];
        }
      }
    }
    return inputs;
  });
}

} // namespace bench::bits

#endif
