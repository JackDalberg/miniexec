#pragma once

#include <condition_variable>
#include <exception>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

#include "core.hpp"

namespace miniexec {

struct sync_wait_env {
  std::stop_token token;

  std::stop_token get_stop_token() const noexcept { return token; }
};

template <typename Value>
struct sync_wait_state {
  std::mutex mutex;
  std::condition_variable cv;
  std::optional<Value> value;
  std::exception_ptr err;
  bool stopped = false;
  bool done = false;
  std::stop_token token;
};

template <typename State>
struct sync_wait_receiver {
  State* state;

  template <typename... As>
  void set_value(As&&... as) noexcept {
    {
      std::lock_guard lock(state->mutex);
      state->value.emplace(std::forward<As>(as)...);
      state->done = true;
    }
    state->cv.notify_one();
  }

  void set_error(std::exception_ptr e) noexcept {
    {
      std::lock_guard lock(state->mutex);
      state->err = std::move(e);
      state->done = true;
    }
    state->cv.notify_one();
  }

  void set_stopped() noexcept {
    {
      std::lock_guard lock(state->mutex);
      state->stopped = true;
      state->done = true;
    }
    state->cv.notify_one();
  }

  sync_wait_env get_env() const noexcept { return sync_wait_env{state->token}; }
};

template <sender S>
auto sync_wait(S&& s) {
  using value_t = typename std::remove_cvref_t<S>::value_t;

  sync_wait_state<value_t> state;
  std::stop_source source;
  state.token = source.get_token();

  auto op = connect(std::forward<S>(s),
                    sync_wait_receiver<sync_wait_state<value_t>>{&state});
  start(op);

  std::unique_lock lock(state.mutex);
  state.cv.wait(lock, [&] { return state.done; });

  if (state.err) std::rethrow_exception(state.err);

  if (state.stopped) return std::optional<value_t>{};

  return std::move(state.value);
}

}  // namespace miniexec
