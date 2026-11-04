#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "core.hpp"

namespace miniexec {

template <typename F, typename Tuple>
struct let_value_result {};

template <typename F, typename... Ts>
struct let_value_result<F, std::tuple<Ts...>> {
  using sender_t = std::decay_t<std::invoke_result_t<F&, Ts...>>;
  using value_t = typename sender_t::value_t;
  static_assert(sender<sender_t>);
};

template <typename Op>
struct let_value_receiver {
  Op* parent;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    parent->start_successor(std::forward<As>(as)...);
  }

  void set_error(std::exception_ptr e) noexcept {
    parent->receiver.set_error(std::move(e));
  }

  void set_stopped() noexcept { parent->receiver.set_stopped(); }

  auto get_env() const noexcept { return parent->receiver.get_env(); }
};

template <typename Receiver>
struct let_value_successor_receiver {
  Receiver* receiver;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    receiver->set_value(std::forward<As>(as)...);
  }

  void set_error(std::exception_ptr e) noexcept {
    receiver->set_error(std::move(e));
  }

  void set_stopped() noexcept { receiver->set_stopped(); }

  auto get_env() const noexcept { return receiver->get_env(); }
};

template <typename Pred, typename F, typename Receiver>
struct let_value_operation {
  using result_t = let_value_result<F, typename Pred::value_t>;
  using successor_sender_t = typename result_t::sender_t;
  using successor_receiver_t = let_value_successor_receiver<Receiver>;
  using successor_op_t =
      connect_result_t<successor_sender_t, successor_receiver_t>;
  using predecessor_receiver_t = let_value_receiver<let_value_operation>;
  using predecessor_op_t = connect_result_t<Pred, predecessor_receiver_t>;

  predecessor_op_t pred_op;
  F f;
  Receiver receiver;
  alignas(successor_op_t) std::byte storage[sizeof(successor_op_t)];
  bool active = false;

  let_value_operation(Pred pred_, F f_, Receiver r)
      : pred_op(std::move(pred_).connect(predecessor_receiver_t{this})),
        f(std::move(f_)),
        receiver(std::move(r)) {}

  ~let_value_operation() {
    if (active)
      std::destroy_at(std::launder(reinterpret_cast<successor_op_t*>(storage)));
  }

  void start() noexcept { miniexec::start(pred_op); }

  template <typename... As>
  void start_successor(As&&... as) noexcept {
    try {
      auto successor_sender = std::invoke(f, std::forward<As>(as)...);
      // Placement new via ::new means no allocations?
      auto* op = ::new (static_cast<void*>(storage)) successor_op_t(
          std::move(successor_sender).connect(successor_receiver_t{&receiver}));
      active = true;
      op->start();
    } catch (...) {
      receiver.set_error(std::current_exception());
    }
  }
};

template <typename Pred, typename F>
struct let_value_sender {
  using value_t = typename let_value_result<F, typename Pred::value_t>::value_t;

  Pred pred;
  F f;

  template <typename Receiver>
  auto connect(Receiver&& receiver) && {
    return let_value_operation<Pred, F, std::decay_t<Receiver>>{
        std::move(pred), std::move(f), std::forward<Receiver>(receiver)};
  }

  template <typename Receiver>
  auto connect(Receiver&& receiver) const& {
    return let_value_operation<Pred, F, std::decay_t<Receiver>>{
        pred, f, std::forward<Receiver>(receiver)};
  }
};

template <typename F>
struct let_value_closure : sender_adapter_closure<let_value_closure<F>> {
  F f;

  explicit let_value_closure(F f_) : f(std::move(f_)) {}

  template <sender S>
  auto operator()(S&& s) && -> let_value_sender<std::decay_t<S>, F> {
    return let_value_sender<std::decay_t<S>, F>{std::forward<S>(s),
                                                std::move(f)};
  }

  template <sender S>
  auto operator()(S&& s) const& -> let_value_sender<std::decay_t<S>, F> {
    return let_value_sender<std::decay_t<S>, F>{std::forward<S>(s), f};
  }
};

template <typename F>
auto let_value(F&& f) {
  return let_value_closure<std::decay_t<F>>{std::forward<F>(f)};
}

}  // namespace miniexec
