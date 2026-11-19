#ifndef _INTERPRETER_H_
#define _INTERPRETER_H_

#include <cassert>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../utils/bench.hh"
#include "parser.h"

template <typename NFA> struct Instance {
  /// Reads the input automata, one per file, all at once (bitvector inputs
  /// may need one alphabet); the tool times its parsing and construction.
  std::function<std::vector<NFA>(const std::vector<std::string> &files)> load;
  std::function<NFA(const NFA &, const NFA &)> intersection;
  std::function<NFA(const std::vector<NFA> &)> inter_all;
  std::function<NFA(const NFA &, const NFA &)> uni;
  std::function<NFA(const std::vector<NFA> &)> uni_all;
  std::function<NFA(const NFA &)> complement;
  std::function<NFA(const NFA &, const NFA &)> concat;
  std::function<bool(const NFA &)> is_empty;
  std::function<NFA(const NFA &)> determinize;
  std::function<NFA(const NFA &)> minimize;
  std::function<bool(const NFA &, const NFA &)> is_included;
  /// Accepting words with one given length per tape; false when there are none.
  std::function<bool(const NFA &, const std::vector<unsigned> &)>
      words_of_lengths;
};

template <typename NFA> struct Interpreter {
  int param_cnt = 0;
  Instance<NFA> instance{};
  std::vector<SSA_Call> program{};
  std::map<std::string, NFA> aut_table{};
  std::vector<std::string> arguments;
  std::vector<std::string> load_all_auts{};
  std::vector<NFA> loaded{};

  Interpreter(const Instance<NFA> &inst, const std::vector<std::string> &args)
      : instance(inst), param_cnt(0), program(), aut_table(), arguments(args) {}

public:
  void run_program(std::ifstream &filename) {
    this->program = parse_program(filename);
    this->param_cnt = 0;
    // An operation the engine lacks fails the whole run before anything is
    // timed, rather than being skipped or failing midway.
    for (const SSA_Call &call : this->program) {
      if (!supports(call.operation)) {
        throw std::runtime_error("operation '" + op_name(call.operation) +
                                 "' is not supported by this interpreter");
      }
    }

    // Every input automaton is read before the program runs.
    this->loaded = this->instance.load(this->arguments);
    if (this->loaded.size() != this->arguments.size()) {
      throw std::runtime_error(
          "loaded " + std::to_string(this->loaded.size()) + " automata from " +
          std::to_string(this->arguments.size()) + " files");
    }

    for (size_t i = 0; i < this->program.size(); i++) {
      const OPERATION op{this->program[i].operation};
      if (op == LOAD_CMD) {
        load_nfa(this->program[i].result.value(), this->param_cnt++);
      } else if (op == LOAD_ALL) {
        load_all();
      } else {
        // The time of every operation of the program, summed over the run: the
        // work of the library, whatever the tool names its steps.
        bench::measure("operation", [&] { run_operation(this->program[i]); });
      }
    }
  }

  bool supports(OPERATION op) const {
    switch (op) {
    case LOAD_CMD:
    case LOAD_ALL:
      return static_cast<bool>(this->instance.load);
    case INTERSECTION:
      return static_cast<bool>(this->instance.intersection);
    case INTERALL:
      return static_cast<bool>(this->instance.inter_all);
    case UNION:
      return static_cast<bool>(this->instance.uni);
    case UNIALL:
      return static_cast<bool>(this->instance.uni_all);
    case COMPLEMENT:
      return static_cast<bool>(this->instance.complement);
    case CONCAT:
      return static_cast<bool>(this->instance.concat);
    case EMPTINESS_CHECK:
      return static_cast<bool>(this->instance.is_empty);
    case DETERMINIZE:
      return static_cast<bool>(this->instance.determinize);
    case MINIMIZE:
      return static_cast<bool>(this->instance.minimize);
    case INCLUSION_CHECK:
      return static_cast<bool>(this->instance.is_included);
    case WORDS_OF_LENGTHS:
      return static_cast<bool>(this->instance.words_of_lengths);
    default: // e.g. load_regex: parsed, but no interpreter runs it
      return false;
    }
  }

  static std::string op_name(OPERATION op) {
    for (const auto &[name, value] : STR_OP) {
      if (value == op) {
        return name;
      }
    }
    return std::to_string(op);
  }

private:
  void run_operation(const SSA_Call &call) {
    switch (call.operation) {
    case INTERSECTION:
      intersection(call.result.value(), call.params);
      break;
    case INTERALL:
      inter_all(call.result.value());
      break;
    case UNION:
      uni(call.result.value(), call.params);
      break;
    case UNIALL:
      uni_all(call.result.value());
      break;
    case COMPLEMENT:
      complement(call.result.value(), call.params);
      break;
    case CONCAT:
      concat(call.result.value(), call.params);
      break;
    case EMPTINESS_CHECK:
      is_empty(call.params[0]);
      break;
    case DETERMINIZE:
      determinize(call.result.value(), call.params);
      break;
    case MINIMIZE:
      minimize(call.result.value(), call.params);
      break;
    case INCLUSION_CHECK:
      is_included(call.params[0], call.params[1]);
      break;
    case WORDS_OF_LENGTHS:
      words_of_lengths(call.params);
      break;
    default:
      throw std::runtime_error("operation '" + op_name(call.operation) +
                               "' cannot be run");
    }
  }

  void load_nfa(const std::string &name, int index) {
    this->aut_table[name] = this->loaded.at(index);
  }

  void load_all() {
    for (size_t i = 0; i < this->loaded.size(); i++) {
      std::string name = "_aut{}" + std::to_string(i);
      this->aut_table[name] = this->loaded[i];
      this->load_all_auts.push_back(name);
    }
  }

  // 'load_automata' binds every input, so the operations on all of them take
  // the loaded automata themselves.
  void inter_all(const std::string &res) {
    this->aut_table[res] = this->instance.inter_all(this->loaded);
  }

  // Folds @p op over the operands from the left; the first pair is applied to
  // the bound automata themselves, so no operand is copied.
  template <typename Op>
  NFA fold(const std::vector<std::string> &params, const Op &op) {
    assert(params.size() > 0);
    if (params.size() == 1) {
      return this->aut_table.at(params[0]);
    }
    NFA tmp = op(this->aut_table.at(params[0]), this->aut_table.at(params[1]));
    for (size_t i = 2; i < params.size(); i++) {
      tmp = op(tmp, this->aut_table.at(params[i]));
    }
    return tmp;
  }

  void intersection(const std::string &res,
                    const std::vector<std::string> &params) {
    this->aut_table[res] = fold(params, this->instance.intersection);
  }

  void uni(const std::string &res, const std::vector<std::string> &params) {
    this->aut_table[res] = fold(params, this->instance.uni);
  }

  void uni_all(const std::string &res) {
    this->aut_table[res] = this->instance.uni_all(this->loaded);
  }

  void concat(const std::string &res, const std::vector<std::string> &params) {
    this->aut_table[res] = fold(params, this->instance.concat);
  }

  void complement(const std::string &res,
                  const std::vector<std::string> &params) {
    assert(params.size() == 1);
    this->aut_table[res] =
        this->instance.complement(this->aut_table.at(params[0]));
  }

  void is_included(const std::string &a1, const std::string &a2) {
    this->instance.is_included(this->aut_table.at(a1), this->aut_table.at(a2));
  }

  void is_empty(const std::string &name) {
    this->instance.is_empty(this->aut_table.at(name));
  }

  void determinize(const std::string &res,
                   const std::vector<std::string> &params) {
    assert(params.size() == 1);
    this->aut_table[res] =
        this->instance.determinize(this->aut_table.at(params[0]));
  }
  void minimize(const std::string &res,
                const std::vector<std::string> &params) {
    assert(params.size() == 1);
    this->aut_table[res] =
        this->instance.minimize(this->aut_table.at(params[0]));
  }

  // The literal arguments are the lengths, one per tape, in tape order.
  void words_of_lengths(const std::vector<std::string> &params) {
    if (params.size() < 2) {
      throw std::runtime_error(
          "words_of_lengths expects an automaton and at least one length");
    }
    std::vector<unsigned> lengths{};
    lengths.reserve(params.size() - 1);
    for (size_t i = 1; i < params.size(); ++i) {
      try {
        lengths.push_back(static_cast<unsigned>(std::stoul(params[i])));
      } catch (const std::exception &) {
        throw std::runtime_error("words_of_lengths: '" + params[i] +
                                 "' is not a length");
      }
    }
    this->instance.words_of_lengths(this->aut_table.at(params[0]), lengths);
  }
};

#endif
