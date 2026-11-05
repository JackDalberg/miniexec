#pragma once

#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>

namespace miniexec {

template <typename Receiver, typename... Ts>
struct just_operator {
  std::tuple<Ts...> values;
  Receiver receiver;

  void start() noexcept {
    std::apply(
        [this](auto&&... args) {
          receiver.set_value(std::forward<decltype(args)>(args)...);
        },
        std::move(values));
  }
};

template <typename... Ts>
struct just_sender {
  using value_t = std::tuple<Ts...>;

  std::tuple<Ts...> values;

  template <typename Receiver>
  auto connect(Receiver&& receiver) && {
    return just_operator<std::decay_t<Receiver>, Ts...>{
        std::move(values), std::forward<Receiver>(receiver)};
  }

  template <typename Receiver>
  auto connect(Receiver&& receiver) const& {
    return just_operator<std::decay_t<Receiver>, Ts...>{
        values, std::forward<Receiver>(receiver)};
  }
};

struct just_t {
  template <typename... Ts>
  auto operator()(Ts&&... values) {
    return just_sender<std::decay_t<Ts>...>{
        std::tuple<std::decay_t<Ts>...>{std::forward<Ts>(values)...}};
  }
};
inline constexpr just_t just{};

template <typename Receiver>
struct just_error_operator {
  std::exception_ptr err;
  Receiver receiver;

  void start() noexcept { receiver.set_error(std::move(err)); }
};

struct just_error_sender {
  using value_t = std::tuple<>;

  std::exception_ptr err;

  template <typename Receiver>
  auto connect(Receiver&& receiver) &&;

  template <typename Receiver>
  auto connect(Receiver&& receiver) const&;
};

struct just_error_t {
  auto operator()(std::exception_ptr err) const {
    return just_error_sender{std::move(err)};
  }
};
inline constexpr just_error_t just_error{};

template <typename Receiver>
struct just_stopped_operator {
  Receiver receiver;

  void start() noexcept { receiver.set_stopped(); }
};

struct just_stopped_sender {
  using value_t = std::tuple<>;

  template <typename Receiver>
  auto connect(Receiver&& receiver) &&;

  template <typename Receiver>
  auto connect(Receiver&& receiver) const&;
};

struct just_stopped_t {
  auto operator()() const { return just_stopped_sender{}; }
};
inline constexpr just_stopped_t just_stopped{};

}  // namespace miniexec
