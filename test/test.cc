#include "fmt_async.h"
#include "log.h"
#include "log_helper.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fmt/compile.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <string_view>
#include <utility>

using context = fmt::format_context;

struct test_object {
  int x;
  int y;
};
template <> struct fmt::formatter<test_object> : formatter<string_view> {
  auto format(const test_object &t, fmt::format_context &ctx) {
    return fmt::format_to(ctx.out(), "{} {}", t.x, t.y);
  }
};
struct test_custom {
  int x;
  int y;
};

template <> struct fmt::formatter<test_custom> : formatter<string_view> {
  auto format(const test_custom &t, fmt::format_context &ctx) {
    return ctx.out();
  }
};
template <> struct async_logger::custom_store<test_custom> {
  static size_t alloc_size(const test_custom &t) { return strlen("hello"); };
  static char *store(char *&buf, test_custom &t) {
    buf = stpcpy(buf, "hello");
    return buf;
  }
};

TEST(async_logger, trans_arg_type) {
  using namespace async_logger::detail;
  static_assert(std::is_same_v<transformed_arg_type<std::string, context>,
                               fmt::basic_string_view<char>>);
  static_assert(std::is_same_v<transformed_arg_type<int, context>, int>);

  static_assert(std::is_same_v<transformed_arg_type<test_object, context>,
                               const test_object &>);
  static_assert(
      std::is_same_v<transformed_arg_type<test_custom, context>, char *>);
  // test named arg transform
  static_assert(
      std::is_same_v<
          transformed_arg_type<fmt::detail::named_arg<char, int>, context>,
          fmt::detail::named_arg<char, int>>);
  static_assert(std::is_same_v<
                transformed_arg_type<fmt::detail::named_arg<char, std::string>,
                                     context>,
                fmt::detail::named_arg<char, fmt::basic_string_view<char>>>);
  static_assert(std::is_same_v<
                transformed_arg_type<fmt::detail::named_arg<char, const char *>,
                                     context>,
                fmt::detail::named_arg<char, fmt::basic_string_view<char>>>);
  static_assert(
      std::is_same_v<
          transformed_arg_type<fmt::detail::named_arg<char, std::string_view>,
                               context>,
          fmt::detail::named_arg<char, fmt::basic_string_view<char>>>);
  static_assert(
      std::is_same_v<transformed_arg_type<
                         fmt::detail::named_arg<char, test_object>, context>,
                     fmt::detail::named_arg<char, const test_object &>>);
  static_assert(
      std::is_same_v<transformed_arg_type<
                         fmt::detail::named_arg<char, test_custom>, context>,
                     fmt::detail::named_arg<char, char *>>);
}

TEST(async_logger, arg_transformer) {
  using namespace async_logger::detail;
  static_assert(
      arg_transformer<int, int, test_object>::get_obj_sum_size<context, 2>() ==
      sizeof(test_object));
  static_assert(arg_transformer<int, test_object,
                                test_object>::get_obj_sum_size<context, 2>() ==
                2 * sizeof(test_object));
  static_assert(arg_transformer<int, test_object,
                                test_object>::get_obj_offset<context, 2>() ==
                sizeof(test_object));
  static_assert(arg_transformer<char *, int, char *, char *, int,
                                char *>::get_cstring_idx<context, 5>() == 4);
  static_assert(arg_transformer<char *, int, char *, char *, int,
                                char *>::get_cstring_idx<context, 3>() == 3);
}

TEST(async_logger, alloc_size) {
  using namespace async_logger;
  auto base = sizeof(basic_async_entry<context>);
  auto value_size = sizeof(fmt::detail::value<context>);
  size_t cstring_size[2];
  detail::stored_as_numeric<int, context>();
  auto s = alloc_size<context>(cstring_size, 1, "TEST", 1, 1, "TEST2");
  ASSERT_EQ(s, base + 5 * value_size + strlen("TEST") + strlen("TEST2"));
  ASSERT_EQ(cstring_size[0], 4);
  ASSERT_EQ(cstring_size[1], 5);
  test_custom t;
  s = alloc_size<fmt::format_context>(cstring_size, 1, t);
  ASSERT_EQ(s, base + 2 * value_size + strlen("hello"));

  // test named arg
  s = alloc_size<context>(cstring_size, 1, "TEST", 1, 1,
                          fmt::arg("test2", "test2"));
  using arg_store = format_arg_store<
      context, int, fmt::basic_string_view<char>, int, int,
      fmt::detail::named_arg<char, fmt::basic_string_view<char>>>;
  ASSERT_EQ(s, base + sizeof(arg_store) + strlen("TEST") + strlen("test2") +
                   sizeof(fmt::basic_string_view<char>));
}

#define TWENTY_ARGS                                                            \
  "{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} "
static const char multiple_brackets[] = TWENTY_ARGS;
static fmt::string_view get_format_string(size_t num_args) {
  return fmt::string_view(&multiple_brackets[(20 - num_args) * 3],
                          num_args * 3);
}

template <typename S, typename... Args>
void test_arg_store(const S &fmt, Args &&...args) {
  using namespace async_logger;
  using namespace fmt::literals;
  char buf[1024];
  std::string target =
      fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
  auto entry = (async_entry<context> *)(buf);
  constexpr size_t num_cstring =
      fmt::detail::count<detail::is_cstring<context, Args>()...>();
  size_t cstring_sizes[std::max(num_cstring, (size_t)1)];
  size_t s = alloc_size<fmt::format_context>(cstring_sizes, args...);
  auto store_size =
      store((void *)buf, fmt, cstring_sizes, std::forward<Args>(args)...);
  std::string test = format(*entry);
  ASSERT_EQ(store_size, s);
  ASSERT_EQ(test, target);
}

TEST(async_logger, arg_store) {
  using namespace async_logger;
  using namespace fmt::literals;
  test_arg_store("{} {} {}", 1, 2, "test");
  test_arg_store(get_format_string(18), short(1), (unsigned short)2, 3, 4U, 5L,
                 6UL, 7LL, 8ULL, 9.0F, 10.0, 11, 12, 13, 14, 15, 16, 17, 18);
  test_arg_store("The answer of {}*{a} is {product}", 6, "product"_a = 42,
                 "a"_a = 7);
  test_arg_store("Hello, {name}! The answer is {number}. Goodbye {name}.",
                 "name"_a = "World", "number"_a = 42);
  test_object obj{1, 2};
  test_arg_store("{}", obj);
  test_custom custom{1, 2};
  test_arg_store("{}", custom);
}

struct test_dtor {
  test_dtor(int val) : v(val) {}
  ~test_dtor() { dtor_cnt++; }
  int v;
  static int dtor_cnt;
};

int test_dtor::dtor_cnt = 0;

struct test_move {
public:
  test_move(int v = 0) : val{test_dtor(v)} {}

  std::vector<test_dtor> val;
};

template <> struct fmt::formatter<test_move> : formatter<int> {
  template <typename FormatContext>
  auto format(const test_move &val, FormatContext &ctx) {
    return formatter<int>::format(val.val[0].v, ctx);
  }
};

template <> struct fmt::formatter<test_dtor> : formatter<int> {
  template <typename FormatContext>
  auto format(const test_dtor &val, FormatContext &ctx) const {
    return formatter<int>::format(val.v, ctx);
  }
};

TEST(async_logger, dtor) {
  using namespace async_logger;
  static_assert(detail::is_need_destruct<test_dtor, context>());
  static_assert(detail::is_need_destruct<test_dtor &, context>());
  static_assert(detail::is_need_destruct<test_dtor &&, context>());
  {
    test_dtor d{0};
    test_arg_store("{} {}", d, d);
    ASSERT_EQ(test_dtor::dtor_cnt, 2);
  }
  // notice: life cycle not deep copy
  std::vector<int> v{1, 2, 3};
  test_arg_store("{} ", v);
  test_arg_store("{} ", std::vector<int>{1, 2, 3});
  int dtor_cnt;
  {
    test_move val(456);
    dtor_cnt = test_dtor::dtor_cnt;
    test_arg_store("test move: {}", std::move(val));
    ASSERT_EQ(test_dtor::dtor_cnt, dtor_cnt + 1);
    ASSERT_EQ(val.val.size(), 0);
    dtor_cnt = test_dtor::dtor_cnt;
  }
  ASSERT_EQ(test_dtor::dtor_cnt, dtor_cnt);
}

TEST(async_logger, log) {
  using namespace async_logger;
  SET_LOG_FILE("test.log");

  ASYNC_LOG(LOG_DEBUG, "{} {} {}", 1, "test", "test2");
  std::string s = "test";
  const char buf[] = "const buf array";
  char buf2[64];
  strcpy(buf2, "buf array");
  ASYNC_LOG(LOG_DEBUG,
            "str:{} c_str:{} string_view:{} char_buf:{} char_array:{}", s,
            s.c_str(), std::string_view(s), buf, buf2);
  POLL();
  auto t0 = detail::get_current_nano_sec();
  for (int i = 0; i < 10000; i++) {
    ASYNC_LOG(LOG_DEBUG, "log message with one parameters {}", 1);
  }
  auto t1 = detail::get_current_nano_sec();
  std::cout << (t1 - t0) / 10000.0 << std::endl;
}

TEST(async_logger, log_kv) {
  using namespace async_logger;
  ASYNC_LOG_KV(LOG_DEBUG, "event", ("KEY1", 1), ("KEY2", 2));
  POLL();
}

// interface test
class md_stream {
public:
  class i_stream_msg {
  public:
    virtual double get_ask1_p() = 0;
    virtual double get_bid1_p() = 0;
    virtual std::string to_string() const = 0;
    virtual size_t size() const = 0;
    virtual i_stream_msg *clone(char *&buf) = 0;
  };
};

struct md {
  double ask_p1;
  double bid_p1;
  double ask_p2;
  double ask_p3;
  double ask_p4;
  double ask_p5;
  double ask_p6;
  double ask_p7;
  double ask_p8;
  double ask_p9;
  double ask_p10;
  double ask_p11;
  double ask_p12;
  double ask_p13;
  double ask_p14;
  double ask_p15;
};

class md_stream_impl {
public:
  class stream_msg : public md_stream::i_stream_msg {
  public:
    stream_msg(md *f) : f_(f) {}
    virtual double get_ask1_p() { return f_->ask_p1; }
    virtual double get_bid1_p() { return f_->bid_p1; }
    virtual size_t size() const { return sizeof(stream_msg) + sizeof(md); }
    virtual std::string to_string() const {
      return "ask_p1:" + std::to_string(f_->ask_p1);
    }

    virtual i_stream_msg *clone(char *&buf) {
      char *p_start = buf;
      memcpy(buf, f_, sizeof(md));
      buf += sizeof(md);
      auto f = new (buf) stream_msg(*this);
      f->f_ = (md *)p_start;
      buf += sizeof(stream_msg);
      return f;
    }
    md *f_;
  };
};

template <>
struct fmt::formatter<md_stream::i_stream_msg> : formatter<string_view> {
  auto format(const md_stream::i_stream_msg &t, fmt::format_context &ctx) {
    fmt::format_to(ctx.out(), "{}", t.to_string());
    return ctx.out();
  }
};
template <> struct async_logger::custom_store<md_stream::i_stream_msg> {
  static size_t alloc_size(const md_stream::i_stream_msg &t) {
    return t.size();
  };
  static const md_stream::i_stream_msg &store(char *&buf,
                                              md_stream::i_stream_msg &t) {
    return *t.clone(buf);
  }
};

TEST(async_logger, field) {
  POLL();
  using namespace async_logger;
  SET_LOG_FILE("test.log");
  md md_;
  md_.ask_p1 = 1;
  md_.bid_p1 = 1.5;
  md_stream_impl::stream_msg msg(&md_);
  md_stream::i_stream_msg *md_msg = &msg;
  ASYNC_LOG(LOG_DEBUG, "{}", *md_msg);
  POLL();
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  if (RUN_ALL_TESTS() == 0) {
    printf("success\n");
  }
}