#include <cstdio>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "../include/miniexec/execution.hpp"

using namespace std::literals;

int main() {
  miniexec::run_loop loop;

  std::jthread worker([&](std::stop_token st) {
    std::stop_callback cb{st, [&] { loop.finish(); }};
    loop.run();
  });

  miniexec::sender auto hello = miniexec::just("hello world"s);
  miniexec::sender auto print =
      std::move(hello) |
      miniexec::then([](std::string msg) { return std::puts(msg.c_str()); });

  miniexec::scheduler auto io_thread = loop.get_scheduler();
  miniexec::sender auto work = miniexec::on(io_thread, std::move(print));

  auto [result] = miniexec::sync_wait(std::move(work)).value();
  return result;
}
