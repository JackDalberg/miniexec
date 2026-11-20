#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <tuple>
#include <type_traits>
#include <utility>

#include "env.hpp"
#include "scheduler.hpp"
#include "sender.hpp"

namespace miniexec {

template <typename Receiver>
struct run_loop_operation;

class run_loop {
  enum class state {
    starting,
    running,
    finishing,
    finished,
  };

 public:
  using task_t = std::move_only_function<void() noexcept>;

  class scheduler;

  run_loop() = default;
  run_loop(const run_loop&) = delete;
  run_loop& operator=(const run_loop&) = delete;
  run_loop(run_loop&&) = delete;
  run_loop& operator=(run_loop&&) = delete;

  ~run_loop() {
    std::lock_guard lock(mutex_);
    if (state_ == state::running || !tasks_.empty()) std::terminate();
  }

  scheduler get_scheduler() noexcept;

  void run() noexcept {
    std::unique_lock lock(mutex_);

    if (state_ == state::finished) return;

    if (state_ == state::starting) state_ = state::running;

    while (true) {
      cv_.wait(lock, [this]() {
        return !tasks_.empty() || state_ == state::finishing ||
               state_ == state::finished;
      });

      if (tasks_.empty()) {
        if (state_ == state::finishing) state_ = state::finished;
        return;
      }

      auto task = std::move(tasks_.front());
      tasks_.pop();
      lock.unlock();
      task();
      lock.lock();
    }
  }

  void finish() noexcept {
    {
      std::lock_guard lock(mutex_);
      if (state_ == state::starting || state_ == state::running) {
        state_ = state::finishing;
      } else {
        return;
      }
    }
    cv_.notify_all();
  }

 private:
  template <typename Receiver>
  friend struct run_loop_operation;

  void enqueue(task_t task) {
    {
      std::lock_guard lock(mutex_);
      tasks_.push(std::move(task));
    }
    cv_.notify_one();
  }

  std::queue<task_t> tasks_;
  std::mutex mutex_;
  std::condition_variable_any cv_;
  state state_ = state::starting;
};

class run_loop::scheduler {
 public:
  using scheduler_concept = scheduler_tag;

  scheduler() = delete;
  explicit scheduler(run_loop* loop) noexcept : loop_(loop) {}

  auto schedule() const;

  forward_progress_guarantee get_forward_progress_guarantee() const noexcept {
    return forward_progress_guarantee::weakly_parallel;
  }

  friend bool operator==(const scheduler&, const scheduler&) = default;

 private:
  run_loop* loop_;
};

class run_loop_sender {
 public:
  using value_t = std::tuple<>;
  using sender_concept = sender_tag;

  explicit run_loop_sender(run_loop* loop) noexcept : loop_(loop) {}

  template <typename Receiver>
  auto connect(Receiver&& receiver) const;

 private:
  run_loop* loop_;
};

template <typename Receiver>
struct run_loop_operation {
  run_loop* loop;
  Receiver receiver;

  void start() noexcept {
    std::stop_token token;
    if constexpr (requires { miniexec::get_env(receiver).get_stop_token(); })
      token = miniexec::get_env(receiver).get_stop_token();

    if (token.stop_requested()) {
      receiver.set_stopped();
      return;
    }

    loop->enqueue([this, token]() noexcept {
      try {
        if (token.stop_requested()) {
          receiver.set_stopped();
        } else {
          receiver.set_value();
        }
      } catch (...) {
        receiver.set_error(std::current_exception());
      }
    });
  }
};

inline auto run_loop::scheduler::schedule() const {
  return run_loop_sender{loop_};
}

template <typename Receiver>
inline auto run_loop_sender::connect(Receiver&& receiver) const {
  return run_loop_operation<std::decay_t<Receiver>>{
      loop_, std::forward<Receiver>(receiver)};
}

inline run_loop::scheduler run_loop::get_scheduler() noexcept {
  return scheduler{this};
}

}  // namespace miniexec
