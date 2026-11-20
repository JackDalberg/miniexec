#pragma once

#include <concepts>
#include <utility>

namespace miniexec {

template <typename T>
concept queryable = std::destructible<T>;

struct empty_env {};

struct get_env_t {
  template <typename T>
  constexpr auto operator()(T&& t) const noexcept {
    if constexpr (requires { std::forward<T>(t).get_env(); }) {
      return std::forward<T>(t).get_env();
    } else {
      return empty_env{};
    }
  }
};

inline constexpr get_env_t get_env{};

template <typename T>
using env_of_t = decltype(get_env(std::declval<T>()));

struct get_scheduler_t {
  template <typename T>
    requires requires(const T& t) { t.get_scheduler(); }
  constexpr decltype(auto) operator()(const T& t) const noexcept {
    return t.get_scheduler();
  }
};

inline constexpr get_scheduler_t get_scheduler{};

struct get_start_scheduler_t {
  template <typename T>
  constexpr decltype(auto) operator()(const T& t) const noexcept {
    if constexpr (requires { t.get_start_scheduler(); }) {
      return t.get_start_scheduler();
    } else {
      return get_scheduler(t);
    }
  }
};

inline constexpr get_start_scheduler_t get_start_scheduler{};

}  // namespace miniexec
