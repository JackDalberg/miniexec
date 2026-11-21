#include <cstdio>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "../include/miniexec/execution.hpp"
#include "minitest.hpp"

TEST(CppReference, ExecutionMain) {
  std::ostringstream captured;
  {
    CoutCapturer guard(captured.rdbuf());

    using namespace std::literals;
    miniexec::run_loop loop;

    std::jthread worker([&](std::stop_token st) {
      std::stop_callback cb{st, [&] { loop.finish(); }};
      loop.run();
    });

    miniexec::sender auto hello = miniexec::just("hello world"s);
    miniexec::sender auto print =
        std::move(hello) |
        miniexec::then([](std::string msg) { std::cout << msg.c_str(); });

    miniexec::scheduler auto io_thread = loop.get_scheduler();
    miniexec::sender auto work = miniexec::on(io_thread, std::move(print));

    miniexec::sync_wait(std::move(work)).value();
  }
  EXPECT_EQUAL("hello world", captured.str());
}

TEST(CppReference, ExecutionThen) {
  std::ostringstream captured;
  {
    CoutCapturer guard(captured.rdbuf());
    auto sndr = miniexec::just(0) | miniexec::then([](int divisor) {
                  if (divisor == 0)
                    throw std::runtime_error("division by zero");
                  return 10 / divisor;
                }) |
                miniexec::then([](int quotient) {
                  std::cout << "not reached\n";
                  return quotient;
                }) |
                miniexec::upon_error([](std::exception_ptr) { return -1; });

    auto [result] = miniexec::sync_wait(std::move(sndr)).value();
    std::cout << "result: " << result << '\n';
  }
  EXPECT_EQUAL("result: -1\n", captured.str());
}
