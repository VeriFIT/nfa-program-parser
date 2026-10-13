
#ifndef _INTERPRETER_H_
#define _INTERPRETER_H_

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

#include <mata/nfa/nfa.hh>

#include "../utils/mata_load.hh"
#include "parser.h"

template <typename NFA> struct Instance {
  std::function<NFA(const mata::nfa::Nfa &, const std::string &filename)>
      mata_to_nfa;
  std::function<NFA(const NFA &, const NFA &)> intersection;
  std::function<NFA(const std::vector<NFA>)> inter_all;
  std::function<NFA(const NFA &, const NFA &)> uni;
  std::function<NFA(const std::vector<NFA>)> uni_all;
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
  std::vector<mata::nfa::Nfa> inter_auts{};
  std::shared_ptr<mata::OnTheFlyAlphabet> alphabet =
      std::make_shared<mata::OnTheFlyAlphabet>();

  Interpreter(const Instance<NFA> &inst, const std::vector<std::string> &args)
      : instance(inst), param_cnt(0), program(), aut_table(), arguments(args) {}

public:
  void run_program(std::ifstream &filename) {
    this->program = parse_program(filename);
    this->param_cnt = 0;

    // load and minterminize all input automata
    prepare_mata_automata();

    for (size_t i = 0; i < this->program.size(); i++) {
      switch (this->program[i].operation) {
      case LOAD_CMD:
        load_nfa(this->program[i].result.value(), this->param_cnt++);
        break;
      case LOAD_ALL: // all automata are already loaded
        load_all();
        break;
      case INTERSECTION:
        intersection(this->program[i].result.value(), this->program[i].params);
        break;
      case INTERALL:
        inter_all(this->program[i].result.value());
        break;
      case UNION:
        uni(this->program[i].result.value(), this->program[i].params);
        break;
      case UNIALL:
        uni_all(this->program[i].result.value());
        break;
      case COMPLEMENT:
        complement(this->program[i].result.value(), this->program[i].params);
        break;
      case CONCAT:
        concat(this->program[i].result.value(), this->program[i].params);
        break;
      case EMPTINESS_CHECK:
        is_empty(this->program[i].params[0]);
        break;
      case DETERMINIZE:
        determinize(this->program[i].result.value(), this->program[i].params);
        break;
      case MINIMIZE:
        minimize(this->program[i].result.value(), this->program[i].params);
        break;
      case INCLUSION_CHECK:
        is_included(this->program[i].params[0], this->program[i].params[1]);
        break;
      case WORDS_OF_LENGTHS:
        words_of_lengths(this->program[i].params);
        break;
      }
    }
  }

private:
  void prepare_mata_automata() {
    this->inter_auts = bench::load_nfas(this->arguments, this->alphabet);
  }

  void load_nfa(const std::string &name, int index) {
    this->aut_table[name] = this->instance.mata_to_nfa(this->inter_auts[index],
                                                       this->arguments[index]);
  }

  void load_all() {
    for (size_t i = 0; i < this->inter_auts.size(); i++) {
      std::string name = "_aut{}" + std::to_string(i);
      this->aut_table[name] =
          this->instance.mata_to_nfa(this->inter_auts[i], this->arguments[i]);
      this->load_all_auts.push_back(name);
    }
  }

  void inter_all(const std::string &res) {
    std::vector<NFA> nfas{};
    for (const std::string &name : this->load_all_auts) {
      nfas.emplace_back(this->aut_table[name]);
    }
    this->aut_table[res] = this->instance.inter_all(nfas);
  }

  void intersection(const std::string &res,
                    const std::vector<std::string> &params) {
    assert(params.size() > 0);
    NFA tmp = this->aut_table.at(params[0]);
    for (size_t i = 1; i < params.size(); i++) {
      tmp = this->instance.intersection(tmp, this->aut_table.at(params[i]));
    }
    this->aut_table[res] = tmp;
  }

  void uni(const std::string &res, const std::vector<std::string> &params) {
    assert(params.size() > 0);
    NFA tmp = this->aut_table.at(params[0]);
    for (size_t i = 1; i < params.size(); i++) {
      tmp = this->instance.uni(tmp, this->aut_table.at(params[i]));
    }
    this->aut_table[res] = tmp;
  }

  void uni_all(const std::string &res) {
    std::vector<NFA> nfas{};
    for (const std::string &name : this->load_all_auts) {
      nfas.emplace_back(this->aut_table[name]);
    }
    this->aut_table[res] = this->instance.uni_all(nfas);
  }

  void concat(const std::string &res, const std::vector<std::string> &params) {
    assert(params.size() > 0);
    NFA tmp = this->aut_table.at(params[0]);
    for (size_t i = 1; i < params.size(); i++) {
      tmp = this->instance.concat(tmp, this->aut_table.at(params[i]));
    }
    this->aut_table[res] = tmp;
  }

  void complement(const std::string &res,
                  const std::vector<std::string> &params) {
    assert(params.size() == 1);
    NFA tmp = this->aut_table.at(params[0]);
    this->aut_table[res] = this->instance.complement(tmp);
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
    NFA tmp = this->aut_table.at(params[0]);
    this->aut_table[res] = this->instance.determinize(tmp);
  }
  void minimize(const std::string &res,
                const std::vector<std::string> &params) {
    assert(params.size() == 1);
    NFA tmp = this->aut_table.at(params[0]);
    this->aut_table[res] = this->instance.minimize(tmp);
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
