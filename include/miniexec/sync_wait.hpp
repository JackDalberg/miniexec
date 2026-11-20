#pragma once

#include <exception>
#include <optional>
#include <stop_token>
#include <utility>

#include "operation.hpp"
#include "run_loop.hpp"
#include "sender.hpp"

namespace miniexec {

struct sync_wait_env {
  run_loop* loop;
  std::stop_token token;

  std::stop_token get_stop_token() const noexcept { return token; }

  run_loop::scheduler get_scheduler() const noexcept {
    return loop->get_scheduler();
  }
  run_loop::scheduler get_start_scheduler() const noexcept {
    return loop->get_scheduler();
  }
};

template <typename Value>
struct sync_wait_state {
  run_loop loop;
  std::optional<Value> value;
  std::exception_ptr err;
  bool stopped = false;
  std::stop_token token;
};

template <typename State>
struct sync_wait_receiver {
  State* state;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    try {
      state->value.emplace(std::forward<As>(as)...);
    } catch (...) {
      state->err = std::current_exception();
    }
    state->loop.finish();
  }

  void set_error(std::exception_ptr e) noexcept {
    state->err = std::move(e);
    state->loop.finish();
  }

  void set_stopped() noexcept {
    state->stopped = true;
    state->loop.finish();
  }

  sync_wait_env get_env() const noexcept {
    return sync_wait_env{&state->loop, state->token};
  }
};

struct sync_wait_t {
  template <detail::value_sender S>
  auto operator()(S&& s) const {
    using value_t = typename std::remove_cvref_t<S>::value_t;

    sync_wait_state<value_t> state;
    std::stop_source source;
    state.token = source.get_token();

    auto op = miniexec::connect(std::forward<S>(s),
                      sync_wait_receiver<sync_wait_state<value_t>>{&state});
    miniexec::start(op);
    state.loop.run();

    if (state.err) std::rethrow_exception(state.err);

    if (state.stopped) return std::optional<value_t>{};

    return std::move(state.value);
  }
};

inline constexpr sync_wait_t sync_wait{};

}  // namespace miniexec
