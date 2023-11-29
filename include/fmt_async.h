#pragma once

#include "log_helper.h"
#include <cstddef>
#include <cstring>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <type_traits>
namespace async_logger::detail {

template <typename Arg, typename Context>
constexpr decltype(auto) get_raw_type() {
  if constexpr (stored_as_numeric<Arg>() || stored_as_string<Arg>()) {
    return declval<Arg>();
  } else if constexpr (store_as_custom<Arg>()) {
    using CArg = std::remove_cv_t<std::remove_reference_t<Arg>>;
    using Type = decltype(custom_store_type<CArg>::store(
        std::declval<char *&>(), std::declval<CArg &>()));
    return declval<Type>();
  } else {
    return declval<Arg>();
  }
}

template <typename Context, typename... Args>
constexpr size_t get_cstring_num() {
  constexpr size_t num_cstring =
      fmt::detail::count<detail::is_cstring<Context, Args>()...>();
  return num_cstring;
}

template <typename Arg, typename Context>
constexpr decltype(auto) get_trans_unname_type() {
  if constexpr (stored_as_numeric<Arg>()) {
    return declval<Arg>();
  } else if constexpr (stored_as_string<Arg>()) {
    using Type = fmt::basic_string_view<typename Context::char_type>;
    return declval<Type>();
  } else if constexpr (store_as_custom<Arg>()) {
    using CArg = std::remove_cv_t<std::remove_reference_t<Arg>>;
    using Type = decltype(custom_store_type<CArg>::store(
        std::declval<char *&>(), std::declval<CArg &>()));
    if constexpr (stored_as_string<Type>()) {
      using RType = fmt::basic_string_view<typename Context::char_type>;
      return declval<RType>();
    } else {
      return declval<Type>();
    }
  } else {
    return declval<
        std::add_const_t<std::remove_reference_t<std::decay_t<Arg>>> &>();
  }
}

template <typename Arg, typename Context>
constexpr decltype(auto) get_trans_named_arg_type() {
  using arg_type = decltype(fmt::detail::arg_mapper<Context>().map(
      std::declval<const Arg &>()));
  using value_type = std::remove_const_t<std::remove_reference_t<arg_type>>;

  using transformed_unname_arg_type =
      std::remove_const_t<std::remove_reference_t<
          decltype(get_trans_unname_type<value_type, Context>())>>;

  return declval<fmt::detail::named_arg<typename Context::char_type,
                                        transformed_unname_arg_type>>();
}

template <typename Arg, typename Context>
using transformed_arg_type =
    std::conditional_t<is_named_arg<Arg>(),
                       decltype(get_trans_named_arg_type<Arg, Context>()),
                       decltype(get_trans_unname_type<Arg, Context>())>;

template <typename... Args> struct arg_transformer {
  using arg_tuple = std::tuple<Args...>;
  template <size_t N>
  using arg_at = typename std::tuple_element<N, arg_tuple>::type;
  using type_tuple = std::tuple<std::decay_t<Args>...>;
  template <size_t N>
  using type_at = typename std::tuple_element<N, type_tuple>::type;

  template <typename Context, size_t N> static constexpr size_t get_obj_size() {
    if constexpr (is_named_arg<type_at<N>>()) {
      using arg_type = decltype(fmt::detail::arg_mapper<Context>().map(
          std::declval<const type_at<N> &>()));
      if constexpr (stored_as_object<arg_type, Context>()) {
        return sizeof(arg_type);
      }
    } else if constexpr (stored_as_object<type_at<N>, Context>()) {
      return sizeof(type_at<N>);
    }
    return 0;
  }

  template <typename Context, size_t N>
  static constexpr size_t get_cstring_idx() {
    using Type =
        fmt::detail::mapped_type_constant<std::remove_reference_t<arg_at<N>>,
                                          Context>;
    if constexpr (N == 0) {
      return (Type::value == fmt::detail::type::cstring_type);
    } else {
      return get_cstring_idx<Context, N - 1>() +
             (Type::value == fmt::detail::type::cstring_type);
    }
  }

  template <typename Context, size_t N>
  static constexpr size_t get_obj_sum_size() {
    if constexpr (N == 0) {
      return get_obj_size<Context, N>();
    } else {
      return get_obj_sum_size<Context, N - 1>() + get_obj_size<Context, N>();
    }
  }

  template <typename Context, size_t N>
  static constexpr size_t get_obj_offset() {
    if constexpr (N == 0) {
      return 0;
    } else {
      return get_obj_sum_size<Context, N - 1>();
    }
  }
};

template <typename Context, size_t CstringIdx>
static inline constexpr size_t get_arg_sizes(size_t *cstring_size) {
  return 0;
}

template <typename char_type> bool is_wchar(const char_type *arg) {
  if constexpr (std::is_same_v<char_type, wchar_t>) {
    return true;
  }
  return false;
}

template <typename Context, size_t CstringIdx, typename Arg, typename... Args>
static inline constexpr size_t
get_arg_sizes(size_t *cstring_size, const Arg &arg, const Args &...args) {
  if constexpr (is_named_arg<Arg>()) {
    using arg_type = decltype(fmt::detail::arg_mapper<Context>().map(
        std::declval<const Arg &>()));
    using value_type = std::remove_const_t<std::remove_reference_t<arg_type>>;
    if constexpr (stored_as_string<Arg>()) {
      return sizeof(fmt::basic_string_view<typename Context::char_type>) +
             get_arg_sizes<Context, CstringIdx>(cstring_size, arg.value,
                                                args...);
    } else if constexpr (store_as_custom<value_type>()) {
      using rtn_type = decltype(custom_store_type<value_type>::store(
          std::declval<char *&>(), std::declval<value_type &>()));
      if constexpr (stored_as_string<rtn_type>()) {
        return sizeof(fmt::basic_string_view<typename Context::char_type>) +
               get_arg_sizes<Context, CstringIdx>(cstring_size, arg.value,
                                                  args...);
      } else {
        return get_arg_sizes<Context, CstringIdx>(cstring_size, arg.value,
                                                  args...);
      }
    } else {
      return get_arg_sizes<Context, CstringIdx>(cstring_size, arg.value,
                                                args...);
    }
  } else {
    if constexpr (is_cstring<Context, Arg>()) {
      // todo: wchar_t
      size_t len = strlen(arg);
      cstring_size[CstringIdx] = len;
      return len +
             get_arg_sizes<Context, CstringIdx + 1>(cstring_size, args...);
    } else if constexpr (stored_as_string<Arg, Context>()) {
      size_t len = arg.size();
      return len + get_arg_sizes<Context, CstringIdx>(cstring_size, args...);
    } else if constexpr (store_as_custom<Arg>()) {
      return custom_store_type<Arg>::alloc_size(arg) +
             get_arg_sizes<Context, CstringIdx>(cstring_size, args...);
    } else if constexpr (stored_as_numeric<Arg, Context>()) {
      return get_arg_sizes<Context, CstringIdx>(cstring_size, args...);
    } else {
      return sizeof(std::decay_t<Arg>) +
             get_arg_sizes<Context, CstringIdx>(cstring_size, args...);
    }
  }
}
} // namespace async_logger::detail

namespace async_logger {

template <typename Context, typename... Args> class format_arg_store {
public:
  static const size_t num_args = sizeof...(Args);
  static constexpr size_t num_named_args =
      fmt::detail::count_named_args<Args...>();
  static const bool is_packed = num_args <= fmt::detail::max_packed_args;

  using value_type = fmt::conditional_t<is_packed, fmt::detail::value<Context>,
                                        fmt::basic_format_arg<Context>>;

  fmt::detail::arg_data<value_type, typename Context::char_type, num_args,
                        num_named_args>
      data_;

  static constexpr unsigned long long desc =
      (is_packed ? fmt::detail::encode_types<Context, Args...>()
                 : fmt::detail::is_unpacked_bit | num_args) |
      (num_named_args != 0
           ? static_cast<unsigned long long>(fmt::detail::has_named_args_bit)
           : 0);

public:
  template <typename... T>
  FMT_CONSTEXPR FMT_INLINE format_arg_store(T &...args)
      : data_{fmt::detail::make_arg<is_packed, Context>(args)...} {
    if (fmt::detail::const_check(num_named_args != 0))
      fmt::detail::init_named_args(data_.named_args(), 0, 0, args...);
  }
};

template <typename Context> struct basic_async_entry {
  using char_type = typename Context::char_type;
  using format_arg = typename fmt::basic_format_args<Context>::format_arg;
  using arg_destructor = void (*)(void *p);

  fmt::basic_string_view<char_type> format_;
  unsigned long long desc_;
  arg_destructor dtor_;

  FMT_CONSTEXPR basic_async_entry(fmt::basic_string_view<char_type> format)
      : format_(format), desc_(0), dtor_(0) {}
  const format_arg *get_format_args() const;
  size_t get_format_args_num() const;

  template <typename OutIt, typename T = OutIt>
  using enable_out =
      std::enable_if_t<fmt::detail::is_output_iterator<OutIt, char_type>::value,
                       T>;

  void destruct() {
    if (dtor_)
      dtor_(this);
  }

public:
  struct dtor_sentry {
    dtor_sentry(basic_async_entry &entry) : entry_(entry) {}
    ~dtor_sentry() { entry_.destruct(); }
    basic_async_entry &entry_;
  };
  std::basic_string<char_type> format() const {
    detail::args_t args;
    auto tp = detail::steal_impl(args);
    auto &desc = args.*(std::get<0>(tp));
    desc = desc_;
    auto &format_args = args.*(std::get<1>(tp));
    format_args = get_format_args();
    return fmt::vformat(format_, args);
  }

  template <typename OutIt>
  auto format_to(OutIt out) const -> enable_out<OutIt> {
    detail::args_t args;
    auto tp = detail::steal_impl(args);
    auto &desc = args.*(std::get<0>(tp));
    desc = desc_;
    auto &format_args = args.*(std::get<1>(tp));
    format_args = get_format_args();
    return vformat_to(out, format_, args);
  }
};

template <typename Context, typename... Args>
struct async_entry : basic_async_entry<Context> {
  using entry = basic_async_entry<Context>;

  format_arg_store<Context, Args...> arg_store_;

  template <typename S>
  FMT_CONSTEXPR async_entry(const S &format_str, const Args &...args)
      : entry(fmt::detail::to_string_view(format_str)), arg_store_(args...) {
    entry::desc_ = arg_store_.desc;
  }
  FMT_CONSTEXPR void set_dtor(typename entry::arg_destructor dtor) {
    this->dtor_ = dtor;
  }
};

template <typename Context>
inline const typename basic_async_entry<Context>::format_arg *
basic_async_entry<Context>::get_format_args() const {
  union obfuscated_args {
    const fmt::detail::value<Context> *values_;
    const format_arg *args_;
    intptr_t pointer_; // more efficient to add integer with size, as the
                       // compiler is able to avoid emitting branch
  } args;
  auto &entry = static_cast<const async_entry<Context> &>(*this);
  args.values_ = entry.arg_store_.data_.args_;
  if (entry.desc_ & fmt::detail::has_named_args_bit) {
    args.pointer_ += (desc_ & fmt::detail::is_unpacked_bit)
                         ? sizeof(*args.args_)
                         : sizeof(*args.values_);
  }
  return args.args_;
}

template <typename Context, typename... Args> struct async_entry_constructor {
  template <typename S>
  static size_t make_async_entry(void *buf, const S &format_str,
                                 size_t *cstring_sizes, Args &&...args) {
    return async_entry_constructor<Context, Args...>(
               buf, format_str, cstring_sizes, range(),
               std::forward<Args>(args)...)
        .get_total_size();
  }

  using entry =
      async_entry<Context,
                  std::decay_t<detail::transformed_arg_type<Args, Context>>...>;
  using trans = detail::arg_transformer<Args...>;
  using char_type = typename Context::char_type;
  template <size_t N> using arg_at = typename trans::template arg_at<N>;
  template <size_t N> using type_at = typename trans::template type_at<N>;
  template <size_t N>
  using need_destruct =
      std::conditional_t<detail::is_need_destruct<arg_at<N>, Context>(),
                         std::true_type, std::false_type>;
  using range = std::make_index_sequence<sizeof...(Args)>;

  template <typename S, size_t... Indice>
  FMT_CONSTEXPR
  async_entry_constructor(void *buf, const S &format_str, size_t *cstring_sizes,
                          std::index_sequence<Indice...>, Args &&...args)
      : pentry(reinterpret_cast<char *>(buf)), pBuffer(get_buffer_store(buf)) {
    auto p = new (buf) entry(
        format_str, store<Indice>(cstring_sizes, std::forward<Args>(args))...);
    if constexpr (std::disjunction<need_destruct<Indice>...>::value) {
      p->set_dtor(destructor<Indice...>::dtor);
    }
  }

  template <size_t N> static bool destruct(void *p, std::true_type) {
    reinterpret_cast<type_at<N> *>((char *)p + sizeof(entry) +
                                   trans::template get_obj_offset<Context, N>())
        ->~type_at<N>();
    return true;
  }
  template <size_t N> static bool destruct(void *p, std::false_type) {
    return false;
  }
  template <size_t... Indice> struct destructor {
    static void dtor(void *p) {
      ((destruct<Indice>(p, need_destruct<Indice>())), ...);
    }
  };

  template <size_t N>
  detail::transformed_arg_type<arg_at<N>, Context> store(size_t *cstring_sizes,
                                                         arg_at<N> arg) {
    using Arg = arg_at<N>;
    constexpr bool named_arg = detail::is_named_arg<Arg>();
    if constexpr (named_arg) {
      using value_type = std::remove_reference_t<
          std::remove_const_t<decltype(fmt::detail::arg_mapper<Context>().map(
              std::declval<Arg>()))>>;
      if constexpr (detail::store_as_custom<value_type>()) {
        using custom_arg_type =
            std::remove_cv_t<std::remove_reference_t<value_type>>;
        using Type = decltype(custom_store_type<custom_arg_type>::store(
            std::declval<char *&>(), std::declval<custom_arg_type &>()));
        if constexpr (detail::stored_as_string<Type>()) {
          fmt::basic_string_view<char_type> s(
              custom_store_type<custom_arg_type>::store(pBuffer, arg.value),
              custom_store_type<custom_arg_type>::alloc_size(arg.value));
          char *pstart = pBuffer;
          memcpy(pBuffer, &s, sizeof(s));
          pBuffer += sizeof(s);
          return fmt::arg(arg.name,
                          *(fmt::basic_string_view<char_type> *)pstart);
        } else {
          return fmt::arg(arg.name, custom_store_type<custom_arg_type>::store(
                                        pBuffer, arg.value));
        }

      } else if constexpr (detail::stored_as_string<value_type, Context>()) {
        auto s = copy_string_all<N>(
            pBuffer, cstring_sizes,
            fmt::detail::arg_mapper<Context>().map(std::forward<Arg>(arg)));
        char *pstart = pBuffer;
        memcpy(pBuffer, &s, sizeof(s));
        pBuffer += sizeof(s);
        return fmt::arg(arg.name, *(fmt::basic_string_view<char_type> *)pstart);
      } else if constexpr (detail::stored_as_numeric<value_type, Context>()) {
        return std::forward<Arg>(arg);
      } else {
        char *const pobjs = pentry + sizeof(entry);
        char *const pobj = pobjs + trans::template get_obj_offset<Context, N>();
        using arg_type = decltype(fmt::detail::arg_mapper<Context>().map(
            std::declval<const type_at<N> &>()));
        // copy arg
        using Type = std::remove_const_t<std::remove_reference_t<arg_type>>;
        auto p = new (pobj) Type(arg.value);
        return fmt::arg(arg.name, *p);
      }
    } else {
      if constexpr (detail::store_as_custom<Arg>()) {
        using custom_arg_type = std::remove_cv_t<std::remove_reference_t<Arg>>;
        return custom_store_type<custom_arg_type>::store(
            pBuffer, std::forward<Arg>(arg));
      } else if constexpr (detail::stored_as_string<Arg, Context>()) {
        return copy_string_all<N>(
            pBuffer, cstring_sizes,
            fmt::detail::arg_mapper<Context>().map(std::forward<Arg>(arg)));
      } else if constexpr (detail::stored_as_numeric<Arg, Context>()) {
        return std::forward<Arg>(arg);
      } else {
        char *const pobjs = pentry + sizeof(entry);
        char *const pobj = pobjs + trans::template get_obj_offset<Context, N>();
        using Type = type_at<N>;
        auto p = new (pobj) Type(std::forward<Arg>(arg));
        return *p;
      }
    }
  }
  template <size_t N, typename Arg>
  static fmt::basic_string_view<char_type>
  copy_string_all(char *&pBuffer, size_t *cstring_sizes, Arg &&arg) {
    if constexpr (detail::is_cstring<Context, Arg>()) {
      size_t index = trans::template get_cstring_idx<Context, N>() - 1;
      return copy_cstring(
          pBuffer, cstring_sizes[index],
          fmt::detail::arg_mapper<Context>().map(std::forward<Arg>(arg)));
    } else {
      return copy_string(pBuffer, fmt::detail::arg_mapper<Context>().map(
                                      std::forward<Arg>(arg)));
    }
  };

  static fmt::basic_string_view<char_type>
  copy_cstring(char *&pBuffer, size_t len, const char_type *cstr) {
    if constexpr (std::is_same<char_type, wchar_t>::value) {
      // todo: use memcpy
      wchar_t *p_start = reinterpret_cast<wchar_t *>(pBuffer);
      wchar_t *p_end = wcpcpy(p_start, cstr);
      pBuffer = reinterpret_cast<char *>(p_end);
      return fmt::basic_string_view<wchar_t>(p_start, p_end - p_start);
    } else {
      char *p_start = pBuffer;
      memcpy(pBuffer, cstr, len);
      pBuffer += len;
      return fmt::basic_string_view<char>(p_start, len);
    }
  }
  static fmt::basic_string_view<char_type>
  copy_string(char *&pBuffer, fmt::basic_string_view<char_type> sv) {
    char_type *p_start = reinterpret_cast<char_type *>(pBuffer);
    size_t size = sizeof(char_type) * sv.size();
    std::memcpy(p_start, sv.data(), size);
    pBuffer += size;
    return fmt::basic_string_view<char_type>(p_start, sv.size());
  }

  static constexpr size_t get_obj_size() {
    return trans::template get_obj_sum_size<Context, sizeof...(Args) - 1>();
  }

  static FMT_CONSTEXPR char *get_buffer_store(void *buf) {
    char *const pentry =
        reinterpret_cast<char *>(buf); // entry will be constructed here
    char *const pobjs =
        pentry + sizeof(entry); // objects will be stored starting here
    char *const pbufs =
        pobjs + get_obj_size(); // buffers will be stored starting here
    return pbufs;
  }
  constexpr size_t get_total_size() const { return pBuffer - pentry; }
  char *const pentry;
  char *pBuffer;
};

template <typename Context>
inline auto format(basic_async_entry<Context> &entry)
    -> decltype(entry.format()) {
  typename basic_async_entry<Context>::dtor_sentry _(entry);
  return entry.format();
}

template <typename OutIt, typename Context>
inline auto format_to(basic_async_entry<Context> &entry, OutIt out)
    -> decltype(entry.format_to(out)) {
  typename basic_async_entry<Context>::dtor_sentry _(entry);
  return entry.format_to(out);
}

// cstring_sizes avoid strlen twice
template <typename Context, typename... Args>
constexpr size_t alloc_size(size_t *cstring_size, Args &&...args) {
  using entry =
      async_entry<Context,
                  std::decay_t<detail::transformed_arg_type<Args, Context>>...>;
  return detail::get_arg_sizes<Context, 0>(cstring_size, args...) +
         sizeof(entry);
}

template <typename S, typename... Args, typename Char = fmt::char_t<S>>
inline size_t store(void *buf, const S &format_str, size_t *cstring_sizes,
                    Args &&...args) {
  using Context = fmt::format_context;
  using Constructor = async_entry_constructor<Context, Args &&...>;
  return Constructor::make_async_entry(buf, format_str, cstring_sizes,
                                       std::forward<Args>(args)...);
}

template <typename Context, typename... Args>
constexpr size_t alloc_size(Args &&...args) {
  using entry =
      async_entry<Context,
                  std::decay_t<detail::transformed_arg_type<Args, Context>>...>;
  constexpr size_t num_cstring =
      fmt::detail::count<detail::is_cstring<Context, Args>()...>();
  size_t cstring_sizes[std::max(num_cstring, (size_t)1)];
  return detail::get_arg_sizes<Context, 0>(cstring_sizes, args...) +
         sizeof(entry);
}

template <typename S, typename... Args, typename Char = fmt::char_t<S>>
inline size_t store(void *buf, const S &format_str, Args &&...args) {
  using Context = fmt::format_context;
  using Constructor = async_entry_constructor<Context, Args &&...>;
  constexpr size_t num_cstring =
      fmt::detail::count<detail::is_cstring<Context, Args>()...>();
  size_t cstring_sizes[std::max(num_cstring, (size_t)1)];
  alloc_size<Context>(cstring_sizes, args...);
  return Constructor::make_async_entry(buf, format_str, cstring_sizes,
                                       std::forward<Args>(args)...);
}

} // namespace async_logger
