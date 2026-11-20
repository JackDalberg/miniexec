#pragma once

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "env.hpp"
#include "sender.hpp"

namespace miniexec {

class thread_pool {
 public:
  using task_t = std::move_only_function<void() noexcept>;

  thread_pool() {
    auto hw = std::thread::hardware_concurrency();
    auto count = hw > 1 ? hw - 1 : 1;

    workers_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      workers_.emplace_back(
          [this](std::stop_token stop) { worker_loop(stop); });
    }
  }

  ~thread_pool() {
    for (auto& worker : workers_) worker.request_stop();
    cv_.notify_all();
  }

  // No copy or move.
  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;

  void enqueue(task_t task) {
    {
      std::lock_guard lock(mutex_);
      tasks_.push(std::move(task));
    }
    cv_.notify_one();
  }

 private:
  void worker_loop(std::stop_token stop) {
    while (true) {
      task_t task;
      {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, stop, [this, &stop] {
          return !tasks_.empty() || stop.stop_requested();
        });

        if (stop.stop_requested() && tasks_.empty()) return;

        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  std::queue<task_t> tasks_;
  std::mutex mutex_;
  std::condition_variable_any cv_;
  std::vector<std::jthread> workers_;
};

template <typename Receiver>
struct schedule_operation {
  thread_pool* pool;
  Receiver receiver;

  void start() noexcept {
    std::stop_token token;
    if constexpr (requires { get_env(receiver).get_stop_token(); })
      token = get_env(receiver).get_stop_token();

    if (token.stop_requested()) {
      receiver.set_stopped();
      return;
    }

    pool->enqueue([this, token]() noexcept {
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

struct schedule_sender {
  using value_t = std::tuple<>;
  using sender_concept = sender_tag;

  thread_pool* pool;

  template <typename Receiver>
  auto connect(Receiver&& receiver) const {
    return schedule_operation<std::decay_t<Receiver>>{
        pool, std::forward<Receiver>(receiver)};
  }
};

struct schedule_t {
  template <typename S>
    requires requires(S&& s) { std::forward<S>(s).schedule(); }
  decltype(auto) operator()(S&& s) const {
    return std::forward<S>(s).schedule();
  }
};

inline constexpr schedule_t schedule{};

struct scheduler_tag {};

enum class forward_progress_guarantee {
  concurrent,
  parallel,
  weakly_parallel,
};

struct get_forward_progress_guarantee_t {
  template <typename Sch>
    requires requires(const Sch& sch) { sch.get_forward_progress_guaratee(); }
  auto operator()(const Sch& sch) const {
    return sch.get_forward_progress_guaratee();
  }
};

inline constexpr get_forward_progress_guarantee_t
    get_forward_progress_guarantee{};

template <typename Sch>
concept scheduler =
    requires { typename std::remove_cvref_t<Sch>::scheduler_concept; } &&
    std::derived_from<typename std::remove_cvref_t<Sch>::scheduler_concept,
                      scheduler_tag> &&
    std::copyable<std::remove_cvref_t<Sch>> &&
    std::equality_comparable<std::remove_cvref_t<Sch>> && requires(Sch&& sch) {
      { schedule(std::forward<Sch>(sch)) } -> sender;
      {
        get_forward_progress_guarantee(sch)
      } -> std::same_as<forward_progress_guarantee>;
    };

class parallel_scheduler {
 public:
  using scheduler_concept = scheduler_tag;

  explicit parallel_scheduler(thread_pool* pool) : pool_(pool) {}

  auto schedule() const { return schedule_sender{pool_}; }

  forward_progress_guarantee get_forward_progress_guarantee() const noexcept {
    return forward_progress_guarantee::concurrent;
  }

  friend bool operator==(const parallel_scheduler&,
                         const parallel_scheduler&) = default;

 private:
  thread_pool* pool_;
};

inline parallel_scheduler get_parallel_scheduler() {
  static thread_pool* pool = new thread_pool;
  return parallel_scheduler{pool};
}

template <typename Sch>
using schedule_result_t = decltype(schedule(std::declval<Sch>()));

}  // namespace miniexec
