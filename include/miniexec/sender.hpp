#pragma once

#include <concepts>
#include <type_traits>

#include "env.hpp"

namespace miniexec {

struct sender_tag {};

template <typename S>
concept sender =
    requires { typename std::remove_cvref_t<S>::sender_concept; } &&
    std::derived_from<typename std::remove_cvref_t<S>::sender_concept,
                      sender_tag> &&
    requires(const std::remove_cvref_t<S>& s) {
      { get_env(s) } -> queryable;
    } && std::move_constructible<std::remove_cvref_t<S>> &&
    std::constructible_from<std::remove_cvref_t<S>, S>;

namespace detail {

template <typename S>
concept value_sender =
    sender<S> && requires { typename std::remove_cvref_t<S>::value_t; };

}  // namespace detail

}  // namespace miniexec
