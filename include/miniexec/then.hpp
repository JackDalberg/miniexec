#pragma once

#include <exception>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "core.hpp"

namespace miniexec {

template <typename T>
struct then_value {
  using type = std::tuple<std::decay_t<T>>;
};

template <>
struct then_value<void> {
  using type = std::tuple<>;
};

template <typename F, typename... Ts>
using then_value_t = typename then_value<std::invoke_result_t<F&, Ts...>>::type;

template <typename F, typename Tuple>
struct then_sender_value {};

template <typename F, typename... Ts>
struct then_sender_value<F, std::tuple<Ts...>> {
  using type = then_value_t<F, Ts...>;
};

template <typename Op, typename F>
struct then_receiver {
  Op* parent;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    using result_t = std::invoke_result_t<F&, As...>;
    try {
      if constexpr (std::is_void_v<result_t>) {
        std::invoke(parent->f, std::forward<As>(as)...);
        parent->receiver.set_value();
      } else {
        parent->receiver.set_value(
            std::invoke(parent->f, std::forward<As>(as)...));
      }
    } catch (...) {
      parent->receiver.set_error(std::current_exception());
    }
  }

  void set_error(std::exception_ptr e) noexcept {
    parent->receiver.set_error(std::move(e));
  }

  void set_stopped() noexcept { parent->receiver.set_stopped(); }

  auto get_env() const noexcept { return parent->receiver.get_env(); }
};

template <typename Pred, typename F, typename Receiver>
struct then_operation {
  using receiver_t = then_receiver<then_operation, F>;
  using pred_op_t = connect_result_t<Pred, receiver_t>;

  pred_op_t pred_op;
  F f;
  Receiver receiver;

  then_operation(Pred pred_, F f_, Receiver r)
      : pred_op(std::move(pred_).connect(receiver_t{this})),
        f(std::move(f_)),
        receiver(std::move(r)) {}

  void start() noexcept { miniexec::start(pred_op); }
};

template <typename Pred, typename F>
struct then_sender {
  using value_t = typename then_sender_value<F, typename Pred::value_t>::type;

  Pred pred;
  F f;

  template <typename Receiver>
  auto connect(Receiver&& receiver) && {
    return then_operation<Pred, F, std::decay_t<Receiver>>{
        std::move(pred), std::move(f), std::forward<Receiver>(receiver)};
  }

  template <typename Receiver>
  auto connect(Receiver&& receiver) const& {
    return then_operation<Pred, F, std::decay_t<Receiver>>{
        pred, f, std::forward<Receiver>(receiver)};
  }
};

template <typename F>
struct then_closure : sender_adapter_closure<then_closure<F>> {
  F f;

  explicit then_closure(F f_) : f(std::move(f_)) {}

  template <sender S>
  auto operator()(S&& s) && -> then_sender<std::decay_t<S>, F> {
    return then_sender<std::decay_t<S>, F>{std::forward<S>(s), std::move(f)};
  }

  template <sender S>
  auto operator()(S&& s) const& -> then_sender<std::decay_t<S>, F> {
    return then_sender<std::decay_t<S>, F>{std::forward<S>(s), f};
  }
};

template<typename F>
auto then(F&& f) {
  return then_closure<std::decay_t<F>>{std::forward<F>(f)};
}

}  // namespace miniexec
