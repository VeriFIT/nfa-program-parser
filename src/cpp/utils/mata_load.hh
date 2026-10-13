// Loading of the '.mata' NFAs a benchmark program is given, shared by the
// .emp interpreter and by C++ benchmark programs.
#ifndef MATA_LOAD_HH
#define MATA_LOAD_HH

#include <algorithm>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mata/nfa/builder.hh>
#include <mata/nfa/nfa.hh>
#include <mata/parser/inter-aut.hh>
#include <mata/parser/mintermization.hh>
#include <mata/parser/parser.hh>

#include "bench.hh"

namespace bench {

// Nfa::alphabet is a raw pointer in mata < 1.32.33 and a shared_ptr since; the
// overload that does not match the revision in use is simply never called.
inline void share_alphabet(mata::Alphabet *&dst,
                           const std::shared_ptr<mata::OnTheFlyAlphabet> &src) {
  dst = src.get();
}

inline void share_alphabet(std::shared_ptr<mata::Alphabet> &dst,
                           const std::shared_ptr<mata::OnTheFlyAlphabet> &src) {
  dst = src;
}

inline mata::IntermediateAut load_intermediate(const std::string &filename) {
  std::fstream fs(filename, std::ios::in);
  if (!fs) {
    throw std::runtime_error("could not open file: " + filename);
  }
  const mata::parser::Parsed parsed{mata::parser::parse_mf(fs, true)};
  if (parsed.size() != 1) {
    throw std::runtime_error(filename + ": the number of sections is not 1");
  }
  if (!parsed[0].type.starts_with(mata::nfa::TYPE_NFA)) {
    throw std::runtime_error(filename + ": the automaton is not an NFA");
  }
  return mata::IntermediateAut::parse_from_mf(parsed)[0];
}

/// Loads @p files as NFAs over the one @p alphabet, mintermizing them together
/// when any has a bitvector alphabet. Times 'mataparsing' and 'mataminterm'.
inline std::vector<mata::nfa::Nfa>
load_nfas(const std::vector<std::string> &files,
          const std::shared_ptr<mata::OnTheFlyAlphabet> &alphabet) {
  std::vector<mata::IntermediateAut> auts{measure("mataparsing", [&] {
    std::vector<mata::IntermediateAut> parsed{};
    for (const std::string &file : files) {
      parsed.push_back(load_intermediate(file));
    }
    return parsed;
  })};
  const bool bitvector{std::ranges::any_of(auts, [](const auto &aut) {
    return aut.alphabet_type == mata::IntermediateAut::AlphabetType::Bitvector;
  })};
  if (bitvector) {
    auts = measure("mataminterm", [&] { return mata::Mintermization{}.mintermize(auts); });
  }
  std::vector<mata::nfa::Nfa> nfas{};
  for (const mata::IntermediateAut &aut : auts) {
    mata::nfa::Nfa nfa{mata::nfa::builder::construct(aut, alphabet.get())};
    share_alphabet(nfa.alphabet, alphabet);
    nfas.push_back(std::move(nfa));
  }
  return nfas;
}

/// Loads @p files as NFAs over one alphabet that lives as long as the program.
inline std::vector<mata::nfa::Nfa> load_nfas(const std::vector<std::string> &files) {
  static const auto alphabet{std::make_shared<mata::OnTheFlyAlphabet>()};
  return load_nfas(files, alphabet);
}

/// Loads every command-line argument from @p first on as an NFA: the columns of
/// a pycobench input line.
inline std::vector<mata::nfa::Nfa> load_nfas(int argc, char **argv, int first = 1) {
  return load_nfas(std::vector<std::string>(argv + first, argv + argc));
}

} // namespace bench

#endif
