#pragma once

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

namespace miniexec {

class thread_pool {
 public:
  using task_t = std::move_only_function<void() noexcept>;

  thread_pool() {
    auto count = std::thread::hardware_concurrency();
    if (count == 0) count = 1;

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

  thread_pool* pool;

  template <typename Receiver>
  auto connect(Receiver&& receiver) const {
    return schedule_operation<std::decay_t<Receiver>>{
        pool, std::forward<Receiver>(receiver)};
  }
};

class parallel_scheduler {
 public:
  explicit parallel_scheduler(thread_pool* pool) : pool_(pool) {}

  auto schedule() const { return schedule_sender{pool_}; }

 private:
  thread_pool* pool_;
};

struct schedule_t {
  template <typename S>
  auto operator()(S&& s) const -> decltype(std::forward<S>(s).schedule()) {
    return std::forward<S>(s).schedule();
  }
};
inline constexpr schedule_t schedule{};

inline parallel_scheduler get_parallel_scheduler() {
  static thread_pool* pool = new thread_pool;
  return parallel_scheduler{pool};
}

}  // namespace miniexec
