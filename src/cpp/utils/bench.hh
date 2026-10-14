// Measurements of benchmark programs.
//
// Every measurement prints one '<name>: <value>' line on stdout, which pycobench
// turns into the column <name> of the method that ran the program; a name
// printed more than once per run is summed. Times are wall-clock seconds of a
// monotonic clock, the same clock as Python's time.perf_counter() in pyinterpret.
//
//   const Nfa det{bench::measure("determinize", [&] { return determinize(aut); })};
//   bench::measure_repeated("trim", [&] { return trim(aut); });  // sub-millisecond operations
//   bench::report("states", det.num_of_states());
#ifndef BENCH_HH
#define BENCH_HH

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bench {

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::duration<double>;

namespace detail {

// The name becomes a CSV column and part of the report's file names; ':' or ';'
// would also break pycobench's parsing of the line.
inline void check_name(std::string_view name) {
  const auto allowed = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
  };
  if (name.empty() || !std::ranges::all_of(name, allowed)) {
    throw std::invalid_argument("bench: metric name '" + std::string(name) +
                                "' is not [A-Za-z0-9_.-]+");
  }
}

inline void print_seconds(std::string_view name, Seconds elapsed) {
  std::ostringstream value;
  value.precision(9);
  value << elapsed.count();
  std::cout << name << ": " << value.str() << '\n' << std::flush;
}

} // namespace detail

/// Prints '<name>: <value>': a result, a size, anything that is not a time.
/// Booleans print as 'true'/'false', as every interpreter's answers do.
template <typename T> void report(std::string_view name, const T &value) {
  detail::check_name(name);
  if constexpr (std::is_same_v<T, bool>) {
    std::cout << name << ": " << (value ? "true" : "false") << '\n' << std::flush;
  } else {
    std::cout << name << ": " << value << '\n' << std::flush;
  }
}

/// Keeps the compiler from optimising away the computation of @p value.
template <typename T> void keep(const T &value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

/// Runs @p f once, prints its time as @p name and returns its result.
///
/// Destroying the result is not timed. For operations of a millisecond or
/// more; one run of a shorter one is dominated by the clock and by noise.
template <typename F> std::invoke_result_t<F &> measure(std::string_view name, F &&f) {
  detail::check_name(name);
  if constexpr (std::is_void_v<std::invoke_result_t<F &>>) {
    const auto start{Clock::now()};
    std::invoke(f);
    detail::print_seconds(name, Clock::now() - start);
  } else {
    const auto start{Clock::now()};
    std::invoke_result_t<F &> result = std::invoke(f);
    detail::print_seconds(name, Clock::now() - start);
    return result;
  }
}

/// Prints the time of one call of @p f as @p name: the median over batches of
/// at least 10 ms of the mean time per call. Returns the result of an untimed
/// first call that warms the caches and the allocator.
///
/// @p f must give the same result whenever it is called, so it must not
/// modify its inputs: copy them inside @p f (and time the copy) instead.
/// Each timed call includes destroying its result. A call of 10 ms or more is
/// still run seven times; use measure() for those.
template <typename F> std::invoke_result_t<F &> measure_repeated(std::string_view name, F &&f) {
  using Result = std::invoke_result_t<F &>;
  constexpr Clock::duration min_batch{std::chrono::milliseconds{10}};
  constexpr std::size_t batches{5};
  constexpr std::size_t max_calls{std::size_t{1} << 24};
  detail::check_name(name);

  const auto batch = [&f](std::size_t calls) {
    const auto start{Clock::now()};
    for (std::size_t i{0}; i < calls; ++i) {
      if constexpr (std::is_void_v<Result>) {
        std::invoke(f);
      } else {
        keep(std::invoke(f));
      }
    }
    return Clock::now() - start;
  };
  const auto run = [&] {
    std::size_t calls{1};
    while (calls < max_calls && batch(calls) < min_batch) {
      calls *= 2;
    }
    std::vector<Seconds> per_call(batches);
    for (Seconds &time : per_call) {
      time = Seconds{batch(calls)} / static_cast<double>(calls);
    }
    std::ranges::nth_element(per_call, per_call.begin() + batches / 2);
    detail::print_seconds(name, per_call[batches / 2]);
  };

  if constexpr (std::is_void_v<Result>) {
    std::invoke(f);
    run();
  } else {
    Result result = std::invoke(f);
    run();
    return result;
  }
}

} // namespace bench

#endif
