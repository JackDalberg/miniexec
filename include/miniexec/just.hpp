#pragma once

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

template <typename... Ts>
auto just(Ts&&... values) {
  return just_sender<std::decay_t<Ts>...>{
      std::tuple<std::decay_t<Ts>...>{std::forward<Ts>(values)...}};
}

}  // namespace miniexec
