#pragma once
#include "string_literal.h"
#include <cstddef>
#include <fmt/core.h>
#include <tuple>
#include <type_traits>

namespace async_logger {
template <typename T, typename Enable = void> struct custom_store {
  custom_store() = delete;
};
template <typename T> using custom_store_type = custom_store<T>;
} // namespace async_logger
namespace async_logger::detail {
template <typename Arg, typename Context = fmt::format_context>
constexpr bool stored_as_numeric() {
  using Type =
      fmt::detail::mapped_type_constant<std::remove_reference_t<Arg>, Context>;
  return fmt::detail::is_arithmetic_type(Type::value) ||
         Type::value == fmt::detail::type::pointer_type;
}
template <typename Arg, typename Context = fmt::format_context>
constexpr bool stored_as_string() {
  using Type =
      fmt::detail::mapped_type_constant<std::remove_reference_t<Arg>, Context>;
  return Type::value == fmt::detail::type::cstring_type ||
         Type::value == fmt::detail::type::string_type;
}

// check custom
template <typename T> class has_custom_store {
private:
  template <typename U>
  static auto test(int)
      -> decltype(custom_store_type<U>::store(std::declval<char *&>(),
                                              std::declval<T &>()),
                  std::true_type());
  template <typename> static std::false_type test(...);

public:
  static constexpr bool value =
      std::is_same_v<decltype(test<T>(0)), std::true_type>;
};

template <typename T> class has_custom_alloc_size {
private:
  template <typename U>
  static auto test(int) -> typename std::is_same<
      decltype(custom_store_type<U>::alloc_size(std::declval<T &>())),
      size_t>::type;
  template <typename> static std::false_type test(...);

public:
  static constexpr bool value =
      std::is_same_v<decltype(test<T>(0)), std::true_type>;
};

template <typename Arg> constexpr bool store_as_custom() {
  using Type = std::remove_cv_t<std::remove_reference_t<Arg>>;
  return has_custom_store<Type>::value && has_custom_alloc_size<Type>::value;
}

template <typename Arg, typename Context = fmt::format_context>
static inline constexpr bool stored_as_object() {
  return !store_as_custom<Arg>() && !stored_as_numeric<Arg, Context>() &&
         !stored_as_string<Arg, Context>();
}

template <typename Context, typename Arg>
static inline constexpr bool is_cstring() {
  return fmt::detail::mapped_type_constant<std::remove_reference_t<Arg>,
                                           Context>::value ==
         fmt::detail::type::cstring_type;
}
template <typename Arg> static inline constexpr bool is_named_arg() {
  return fmt::detail::is_named_arg<fmt::remove_cvref_t<Arg>>::value;
}

template <typename Arg, typename Context>
static inline constexpr bool is_need_destruct() {
  using Type = std::remove_cv_t<std::remove_reference_t<Arg>>;
  if constexpr (stored_as_object<Type, Context>()) {
    return std::is_destructible_v<Type> &&
           !std::is_trivially_destructible_v<Type>;
  }
  return false;
}

template <size_t Idx, typename Func>
static constexpr decltype(auto) gen_fmt_helper(Func f) {
  return string_literal<0>{};
}

template <size_t Idx, typename Func, typename Arg, typename... Args>
static constexpr auto gen_fmt_helper(Func func) {
  constexpr auto id =
      fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;
  if constexpr (id == fmt::detail::type::string_type ||
                id == fmt::detail::type::cstring_type) {
    constexpr auto e = func()[Idx];
    constexpr int l = e.size();
    constexpr char f[7] = "=\"{}\" ";
    return string_literal<l>{e} + string_literal<6>{f} +
           gen_fmt_helper<Idx + 1, Func, Args...>(func);
  } else {
    constexpr auto e = func()[Idx];
    constexpr int l = e.size();
    constexpr char f[5] = "={} ";
    return string_literal<l>{e} + string_literal<4>{f} +
           gen_fmt_helper<Idx + 1, Func, Args...>(func);
  }
}
template <typename Func, typename... Args>
static constexpr decltype(auto) gen_fmt(Func func, Args &&...args) {
  return gen_fmt_helper<0, Func, Args...>(func);
}

// declval type raw
[[noreturn]] inline void unreachable() {
  // Uses compiler specific extensions if possible.
  // Even if no extension is used, undefined behavior is still raised by
  // an empty function body and the noreturn attribute.
#ifdef __GNUC__ // GCC, Clang, ICC
  __builtin_unreachable();
#elif defined(_MSC_VER) // msvc
  __assume(false);
#endif
}
template <typename T> [[noreturn]] constexpr T declval() { unreachable(); }

// visit private
template <typename T, auto... field> struct thief_member {
  friend auto steal_impl(T &t) { return std::make_tuple(field...); }
};

using args_t = fmt::basic_format_args<fmt::format_context>;
auto steal_impl(args_t &t);
template struct thief_member<args_t, &args_t::desc_, &args_t::args_>;

inline static uint64_t get_current_nano_sec() {
  timespec ts;
  ::clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * 1000000000 + ts.tv_nsec;
}
} // namespace async_logger::detail