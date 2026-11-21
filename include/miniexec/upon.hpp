#pragma once

#include <exception>
#include <functional>
#include <utility>

#include "closure.hpp"
#include "env.hpp"
#include "operation.hpp"
#include "sender.hpp"

namespace miniexec {

template <typename T>
struct upon_error_value {
  using type = std::tuple<std::decay_t<T>>;
};

template <>
struct upon_error_value<void> {
  using type = std::tuple<>;
};

template <typename F>
using upon_error_value_t = typename upon_error_value<
    std::invoke_result_t<F&, std::exception_ptr>>::type;

template <typename Op, typename F>
struct upon_error_receiver {
  Op* parent;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    parent->receiver.set_value(std::forward<As>(as)...);
  }

  void set_error(std::exception_ptr e) noexcept {
    using result_t = std::invoke_result_t<F&, std::exception_ptr>;
    try {
      if constexpr (std::is_void_v<result_t>) {
        std::invoke(parent->f, std::move(e));
        parent->receiver.set_value();
      } else {
        parent->receiver.set_value(std::invoke(parent->f, std::move(e)));
      }
    } catch (...) {
      parent->receiver.set_error(std::current_exception());
    }
  }

  void set_stopped() noexcept { parent->receiver.set_stopped(); }

  auto get_env() const noexcept { return miniexec::get_env(parent->receiver); }
};

template <typename Pred, typename F, typename Receiver>
struct upon_error_operation {
  using receiver_t = upon_error_receiver<upon_error_operation, F>;
  using pred_op_t = connect_result_t<Pred, receiver_t>;

  pred_op_t pred_op;
  F f;
  Receiver receiver;

  upon_error_operation(Pred pred, F f_, Receiver r)
      : pred_op(miniexec::connect(std::move(pred), receiver_t{this})),
        f(std::move(f_)),
        receiver(std::move(r)) {}

  void start() noexcept { miniexec::start(pred_op); }
};

template <typename Pred, typename F>
struct upon_error_sender {
  using pred_value_t = typename Pred::value_t;
  using handler_value_t = upon_error_value_t<F>;
  using value_t = std::conditional_t<detail::error_only_sender<Pred>,
                                     handler_value_t, pred_value_t>;

  using sender_concept = sender_tag;

  Pred pred;
  F f;

  template <typename Receiver>
  auto connect(Receiver&& receiver) && {
    return upon_error_operation<Pred, F, std::decay_t<Receiver>>{
        std::move(pred), std::move(f), std::forward<Receiver>(receiver)};
  }

  template <typename Receiver>
  auto connect(Receiver&& receiver) const& {
    return upon_error_operation<Pred, F, std::decay_t<Receiver>>{
        pred, f, std::forward<Receiver>(receiver)};
  }
};

template <typename F>
struct upon_error_closure : sender_adapter_closure<upon_error_closure<F>> {
    F f;

    explicit upon_error_closure(F f_) : f(std::move(f_)) {}

    template <detail::value_sender S>
    auto operator()(S&& s) && -> upon_error_sender<std::decay_t<S>, F> {
        return upon_error_sender<std::decay_t<S>, F>{std::forward<S>(s), std::move(f)};
    }

    template <detail::value_sender S>
    auto operator()(S&& s) const& -> upon_error_sender<std::decay_t<S>, F> {
        return upon_error_sender<std::decay_t<S>, F>{std::forward<S>(s), f};
    }
};

template <typename F>
auto upon_error(F&& f) {
  return upon_error_closure<std::decay_t<F>>{std::forward<F>(f)};
}

}  // namespace miniexec
