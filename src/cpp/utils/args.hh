// Command line of a C++ benchmark program: '--<key>=<value>' / '--<flag>'
// options (the variant a suite runs it in), then the input automata.
#ifndef BENCH_ARGS_HH
#define BENCH_ARGS_HH

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bench {

class Args {
public:
  Args(int argc, char **argv) {
    int i{1};
    for (; i < argc && std::string_view{argv[i]}.starts_with("--"); ++i) {
      const std::string option{argv[i] + 2};
      const auto eq{option.find('=')};
      options_[option.substr(0, eq)] = eq == std::string::npos ? "" : option.substr(eq + 1);
    }
    files.assign(argv + i, argv + argc);
  }

  /// The value of '--<key>=<value>', or @p fallback without it. Throws on an
  /// option the program does not know, so a misspelt variant fails loudly.
  std::string option(const std::string &key, const std::string &fallback) {
    known_.push_back(key);
    const auto it{options_.find(key)};
    return it == options_.end() ? fallback : it->second;
  }

  /// Whether '--<flag>' was given.
  bool flag(const std::string &key) { return option(key, "\x01") != "\x01"; }

  /// Fails on any option that no option()/flag() call asked for.
  void done() const {
    for (const auto &[key, value] : options_) {
      if (std::find(known_.begin(), known_.end(), key) == known_.end()) {
        throw std::invalid_argument("unknown option --" + key);
      }
    }
  }

  std::vector<std::string> files{};

private:
  std::map<std::string, std::string> options_{};
  std::vector<std::string> known_{};
};

} // namespace bench

#endif
