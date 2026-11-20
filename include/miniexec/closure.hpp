#pragma once

#include <utility>

#include "sender.hpp"

namespace miniexec {

template <typename Derived>
struct sender_adapter_closure {
  template <sender S>
    requires requires(S&& s, Derived&& d) {
      std::forward<Derived>(d)(std::forward<S>(s));
    }
  friend decltype(auto) operator|(S&& s, Derived&& d) {
    return std::forward<Derived>(d)(std::forward<S>(s));
  }

  template <sender S>
    requires requires(S&& s, const Derived& d) { d(std::forward<S>(s)); }
  friend decltype(auto) operator|(S&& s, const Derived& d) {
    return d(std::forward<S>(s));
  }
};

}  // namespace miniexec
