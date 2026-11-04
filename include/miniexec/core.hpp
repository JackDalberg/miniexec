#pragma once

#include <exception>
#include <type_traits>
#include <utility>

namespace miniexec {

struct set_value_t {};
struct set_error_t {};
struct set_stopped_t {};

inline constexpr set_value_t set_value{};
inline constexpr set_error_t set_error{};
inline constexpr set_stopped_t set_stopped{};

template<typename R>
concept receiver = requires(R&& r) {
    std::forward<R>(r).set_error(std::exception_ptr{});
    std::forward<R>(r).set_stopped();
    std::forward<R>(r).get_env();
};

template<typename S>
concept sender = requires {
    typename std::remove_cvref_t<S>::value_t;
};

struct connect_t {
    template<typename S, typename R>
    auto operator()(S&& s, R&& r) const -> decltype(std::forward<S>(s).connect(std::forward<R>(r))) {
        return std::forward<S>(s).connect(std::forward<R>(r));
    }
};
inline constexpr connect_t connect{};

struct start_t {
    template<typename Op>
    auto operator()(Op&& op) const -> decltype(std::forward<Op>(op).start()) {
        return std::forward<Op>(op).start();
    }
};
inline constexpr start_t start{};

struct get_env_t {
    template<typename R>
    auto operator()(R&& r) const -> decltype(std::forward<R>(r).get_env()) {
        return std::forward<R>(r).get_env();
    }
};
inline constexpr get_env_t get_env{};

template<typename S, typename R>
using connect_result_t = decltype(connect(std::declval<S>(), std::declval<R>()));

template<typename Derived>
struct sender_adapter_closure {
    template<sender S>
    friend auto operator|(S&& s, Derived&& d) -> decltype(std::forward<Derived>(d)(std::forward<S>(s))) {
        return std::forward<Derived>(d)(std::forward<S>(s));
    }

    template<sender S>
    friend auto operator|(S&& s, const Derived& d) -> decltype(d(std::forward<S>(s))) {
        return d(std::forward<S>(s));
    }
};

} // namespace miniexec
