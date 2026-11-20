#pragma once

namespace miniexec {

struct set_value_t {};
struct set_error_t {};
struct set_stopped_t {};

inline constexpr set_value_t set_value{};
inline constexpr set_error_t set_error{};
inline constexpr set_stopped_t set_stopped{};

} // namespace miniexec
