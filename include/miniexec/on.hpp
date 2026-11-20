#pragma once

#include <exception>
#include <optional>
#include <tuple>
#include <utility>

#include "env.hpp"
#include "let.hpp"
#include "operation.hpp"
#include "scheduler.hpp"
#include "sender.hpp"

namespace miniexec {

namespace detail {

template <scheduler Sch, value_sender Sender>
auto starts_on(Sch&& sch, Sender&& sender) {
  return schedule(std::forward<Sch>(sch)) |
         let_value([sender = std::forward<Sender>(sender)]() mutable {
           return std::move(sender);
         });
}

template <typename Sender, typename Sch>
struct continues_on_sender;

template <typename Op>
struct continues_on_predecessor_receiver {
  Op* parent;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    try {
      parent->value.emplace(std::forward<As>(as)...);
    } catch (...) {
      parent->error = std::current_exception();
    }
    parent->start_schedule();
  }

  void set_error(std::exception_ptr e) noexcept {
    parent->error = std::move(e);
    parent->start_schedule();
  }

  void set_stopped() noexcept {
    parent->stopped = true;
    parent->start_schedule();
  }

  auto get_env() const noexcept { return miniexec::get_env(parent->receiver); }
};

template <typename Op>
struct continues_on_schedule_receiver {
  Op* parent;

  void set_value() noexcept {
    if (parent->error) {
      parent->receiver.set_error(std::move(parent->error));
    } else if (parent->stopped) {
      parent->receiver.set_stopped();
    } else if (parent->value) {
      std::apply(
          [this](auto&&... as) {
            parent->receiver.set_value(std::forward<decltype(as)>(as)...);
          },
          std::move(*parent->value));
    } else {
      parent->receiver.set_value();
    }
  }

  void set_error(std::exception_ptr e) noexcept {
    parent->receiver.set_error(std::move(e));
  }

  void set_stopped() noexcept { parent->receiver.set_stopped(); }

  auto get_env() noexcept { return miniexec::get_env(parent->receiver); }
};

template <typename Sender, typename Sch, typename Receiver>
struct continues_on_operation {
  using predecessor_receiver_t =
      continues_on_predecessor_receiver<continues_on_operation>;
  using schedule_receiver_t =
      continues_on_schedule_receiver<continues_on_operation>;
  using predecessor_op_t = connect_result_t<Sender, predecessor_receiver_t>;
  using schedule_sender_t = schedule_result_t<Sch>;
  using schedule_op_t =
      connect_result_t<schedule_sender_t, schedule_receiver_t>;

  predecessor_op_t predecessor_op;
  Sch sch;
  Receiver receiver;
  std::optional<typename Sender::value_t> value;
  std::exception_ptr error;
  bool stopped = false;
  std::optional<schedule_op_t> schedule_op;

  continues_on_operation(Sender sender, Sch sch_, Receiver receiver_)
      : predecessor_op(
            miniexec::connect(std::move(sender), predecessor_receiver_t{this})),
        sch(std::move(sch_)),
        receiver(std::move(receiver_)) {}

  void start() noexcept { miniexec::start(predecessor_op); }

  void start_schedule() noexcept {
    try {
      schedule_op.emplace(
          miniexec::connect(schedule(sch), schedule_receiver_t{this}));
      miniexec::start(*schedule_op);
    } catch (...) {
      receiver.set_error(std::current_exception());
    }
  }
};

template <typename Sender, typename Sch>
struct continues_on_sender {
  using value_t = typename Sender::value_t;
  using sender_concept = sender_tag;

  Sender sender;
  Sch sch;

  template <typename Receiver>
  auto connect(Receiver&& receiver) && {
    return continues_on_operation<Sender, Sch, std::decay_t<Receiver>>{
        std::move(sender), std::move(sch), std::forward<Receiver>(receiver)};
  }

  template <typename Receiver>
  auto connect(Receiver&& receiver) const& {
    return continues_on_operation<Sender, Sch, std::decay_t<Receiver>>{
        sender, sch, std::forward<Receiver>(receiver)};
  }
};

template <value_sender Sender, scheduler Sch>
auto continues_on(Sender&& sender, Sch&& sch) {
  return continues_on_sender<std::decay_t<Sender>, std::decay_t<Sch>>{
      std::forward<Sender>(sender), std::forward<Sch>(sch)};
}

}  // namespace detail

template <typename Sch, typename Sender>
struct on_sender {
  using value_t = typename Sender::value_t;
  using sender_concept = sender_tag;

  Sch sch;
  Sender sender;

  template <typename Receiver>
  auto connect(Receiver&& receiver) && {
    auto orig = get_start_scheduler(get_env(receiver));
    return miniexec::connect(
        detail::continues_on(
            detail::starts_on(std::move(sch), std::move(sender)),
            std::move(orig)),
        std::forward<Receiver>(receiver));
  }

  template <typename Receiver>
  auto connect(Receiver&& receiver) const& {
    auto orig = get_start_scheduler(get_env(receiver));
    return miniexec::connect(
        detail::continues_on(detail::starts_on(sch, sender), std::move(orig)),
        std::forward<Receiver>(receiver));
  }
};

struct on_t {
  template <scheduler Sch, detail::value_sender Sender>
  auto operator()(Sch&& sch, Sender&& sender) const
      -> on_sender<std::decay_t<Sch>, std::decay_t<Sender>> {
    return {std::forward<Sch>(sch), std::forward<Sender>(sender)};
  }
};

inline constexpr on_t on{};

}  // namespace miniexec
