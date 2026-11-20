#pragma once

#include <concepts>
#include <utility>

namespace miniexec {

struct connect_t {
  template <typename S, typename R>
    requires requires(S&& s, R&& r) {
      std::forward<S>(s).connect(std::forward<R>(r));
    }
  decltype(auto) operator()(S&& s, R&& r) const {
    return std::forward<S>(s).connect(std::forward<R>(r));
  }
};

inline constexpr connect_t connect{};

struct start_t {
  template <typename Op>
    requires requires(Op& op) {
      { op.start() } -> std::same_as<void>;
    }
  decltype(auto) operator()(Op& op) const {
    return std::forward<Op>(op).start();
  }
};

inline constexpr start_t start{};

template <typename S, typename R>
using connect_result_t =
    decltype(connect(std::declval<S>(), std::declval<R>()));

}  // namespace miniexec
