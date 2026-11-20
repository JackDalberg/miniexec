#pragma once

#include <exception>
#include <utility>

#include "env.hpp"

namespace miniexec {

template <typename R>
concept receiver = requires(R&& r) {
  std::forward<R>(r).set_error(std::exception_ptr{});
  std::forward<R>(r).set_stopped();
  get_env(std::forward<R>(r));
};

}  // namespace miniexec
