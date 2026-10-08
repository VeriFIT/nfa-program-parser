#ifndef _PARSER_EMP_H_
#define _PARSER_EMP_H_

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum OPERATION {
  LOAD_CMD,
  LOAD_ALL,
  LOAD_REGEX,
  INTERSECTION,
  INTERALL,
  UNIALL,
  UNION,
  COMPLEMENT,
  CONCAT,
  DETERMINIZE,
  MINIMIZE,
  EMPTINESS_CHECK,
  INCLUSION_CHECK,
  EQUIVALENCE_CHECK,
  WORDS_OF_LENGTHS,
};

const std::map<std::string, OPERATION> STR_OP = {
    {"load_automaton", LOAD_CMD},
    {"load_automata", LOAD_ALL},
    {"load_regex", LOAD_REGEX},

    {"inter", INTERSECTION},
    {"union", UNION},
    {"compl", COMPLEMENT},
    {"concat", CONCAT},
    {"interall", INTERALL},
    {"unionall", UNIALL},
    {"determinize", DETERMINIZE},
    {"minimize", MINIMIZE},

    {"is_empty", EMPTINESS_CHECK},
    {"incl", INCLUSION_CHECK},
    {"words_of_lengths", WORDS_OF_LENGTHS},
};

typedef std::string Term; // variable or regex

struct SSA_Call {
  OPERATION operation;
  std::optional<Term> result;
  std::vector<Term> params;
};

// Splits a line into tokens; the parentheses that may wrap the arguments of an
// operation carry no meaning and are dropped.
std::vector<std::string> tokenize(const std::string &line) {
  std::string stripped{line};
  std::erase(stripped, '(');
  std::erase(stripped, ')');

  std::istringstream line_stream(stripped);
  std::vector<std::string> tokens{};
  for (std::string token; line_stream >> token;) {
    tokens.push_back(token);
  }
  return tokens;
}

// Parses one '.emp' program: one statement per line, '#' starts a comment line.
//
// A statement is either '<var> = <op> <arg>...', which binds the result of the
// operation to <var>, or '<op> <arg>...', which discards it. An <arg> is the
// name of an automaton bound earlier or, for the operations that take them, a
// literal the operation itself interprets (e.g. the per-tape lengths of
// 'words_of_lengths'). 'load_automaton <var>' is the exception: it names the
// automaton it loads from the next input file and takes no arguments.
std::vector<SSA_Call> parse_program(std::ifstream &input) {
  std::vector<SSA_Call> program{};
  std::string line;

  while (std::getline(input, line)) {
    const std::vector<std::string> tokens{tokenize(line)};
    if (tokens.empty() || tokens.front().starts_with('#')) {
      continue;
    }

    const size_t op_at{(tokens.size() > 1 && tokens[1] == "=") ? 2 : 0};
    if (op_at >= tokens.size()) {
      throw std::runtime_error("no operation after '=' in: " + line);
    }

    const auto op_it{STR_OP.find(tokens[op_at])};
    if (op_it == STR_OP.end()) {
      throw std::runtime_error("unknown operation '" + tokens[op_at] + "'");
    }

    SSA_Call call{op_it->second,
                  {},
                  std::vector<Term>{tokens.begin() + op_at + 1, tokens.end()}};
    if (op_at == 2) {
      call.result = tokens[0];
    }
    if (call.operation == LOAD_CMD && !call.params.empty()) {
      call.result = call.params.front();
      call.params.clear();
    }
    program.push_back(call);
  }

  return program;
}

#endif
