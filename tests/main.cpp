#include <cstdio>
#include <cstdlib>
#include <print>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>

#include "miniexec/execution.hpp"

using namespace miniexec;

namespace {

void check(bool condition, const char* msg) {
  if (condition) return;
  std::printf("Failed: %s\n", msg);
  std::exit(1);
}

struct stopped_sender {
  using value_t = std::tuple<>;

  template <typename Receiver>
  struct operation {
    Receiver receiver;

    void start() noexcept { receiver.set_stopped(); }
  };

  template <typename Receiver>
  auto connect(Receiver&& receiver) const {
    return operation<std::decay_t<Receiver>>{std::forward<Receiver>(receiver)};
  }
};

}  // namespace

int main() {
  {
    auto result = sync_wait(just(1));
    check(result.has_value(), "just returns a value");
    check(std::get<0>(*result) == 1, "just value is correct");
  }

  {
    auto result = sync_wait(just(1) | then([](int x) { return x + 1; }));
    check(result.has_value(), "then returns a value");
    check(std::get<0>(*result) == 2, "then value is correct");
  }

  {
    auto result = sync_wait(just(1) | then([](int) {}));
    check(result.has_value(), "void then returns a value");
    check(std::tuple_size_v<std::remove_reference_t<decltype(*result)>> == 0,
          "void then returns an empty tuple");
  }

  {
    auto result = sync_wait(just(1) | then([](int x) { return x + 1; }) |
                            then([](int x) { return x * 2; }));
    check(result.has_value(), "pipe chain returns a value");
    check(std::get<0>(*result) == 4, "pipe chain value is correct");
  }

  {
    auto result =
        sync_wait(just(1) | let_value([](int x) { return just(x + 1); }));
    check(result.has_value(), "let_value with just returns a value");
    check(std::get<0>(*result) == 2, "let_value with just value is correct");
  }

  {
    auto result = sync_wait(just(1) | let_value([](int x) {
                              return schedule(get_parallel_scheduler()) |
                                     then([x] { return x * 2; });
                            }));
    check(result.has_value(), "let_value with schedule returns a value");
    check(std::get<0>(*result) == 2,
          "let_value with schedule value is correct");
  }

  {
    bool threw = false;
    try {
      (void)sync_wait(just(1) | then([](int) -> int {
                        throw std::runtime_error("then boom");
                        return 0;
                      }));
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw, "then exception propagates");
  }

  {
    bool threw = false;
    try {
      (void)sync_wait(just(1) | let_value([](int) -> just_sender<int> {
                        throw std::runtime_error("let boom");
                        return just(0);
                      }));
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw, "let_value exception propagates");
  }

  {
    auto result = sync_wait(stopped_sender{});
    check(!result.has_value(), "stopped sender returns nullopt");
  }

  {
    std::thread::id worker_id;
    auto result = sync_wait(schedule(get_parallel_scheduler()) | then([&] {
                              worker_id = std::this_thread::get_id();
                            }));
    check(result.has_value(), "schedule completes");
    check(worker_id != std::this_thread::get_id(),
          "schedule runs on a worker thread");
  }

  std::println("All tests passed");
  return 0;
}
