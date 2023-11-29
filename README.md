### ASYNC_LOG

基于fmt的异步日志

#### 特点：

- 基于fmt，提供丰富的format
- 支持自定义结构和自定义formatter
- 支持key-value结构化打印
- 延迟低
- header-only

#### 使用

```C++
include "log.h"
int main() {
    PRE_ALLOC();
    SET_LOG_FILE("test.log");
    SET_LOG_LEVEL(LOG_DEBUG);
    ASYNC_LOG(LOG_DEBUG, "{}", "test");
    ASYNC_LOG_KV(LOG_DEBUG, "test_event", ("KEY_1","VALUE"), ("KEY_2",1));
    POLL();
}
//[2423031] [DEBUG] 2023-11-27T21:58:28.674968396 test/test.cc:349 test
//ts=2023-11-27T21:58:28.674969328 level=DEBUG pid=2423031 file=test/test.cc:350 event="test_event" KEY_1="VALUE" KEY_2=1
```
```
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
  //return type should have fmt::formatter, you can return your custom type
  static char *store(char *&buf, test_custom &t) {
    char *p_start = buf;
    buf = stpcpy(buf, "hello");
    return p_start;
  }
};

struct test_custom_1 {
  int x;
  int y;
};

template <> struct fmt::formatter<test_custom_1> : formatter<string_view> {
  auto format(const test_custom_1 &t, fmt::format_context &ctx) {
    return fmt::format_to(ctx.out(), "{} {}", t.x, t.y);
  }
};
template <> struct async_logger::custom_store<test_custom_1> {
  static size_t alloc_size(const test_custom_1 &t) {
    return sizeof(test_custom_1);
  };
  static test_custom_1 &store(char *&buf, test_custom_1 &t) {
    char *p_start = buf;
    memcpy(buf, &t, sizeof(test_custom_1));
    buf += sizeof(test_custom_1);
    return *(test_custom_1 *)p_start;
  }
};

test_object obj{1, 2};
//store时，在内存中写了hello，可以根据需求自行返回类型，返回的类型将会存在arg store中，如果是custom 需要提供对应的fmt::formatter
test_custom custom{1, 2};
test_custom_1 custom1{1, 2};
ASYNC_LOG(LOG_DEBUG, "{} {} {} {}", 1, obj, custom, custom1);
```
其中：

- POLL接口通常在异步线程调用
- PRE_ALLOC接口提前分配当前进程的队列内存

原理：

延迟格式化，同步打日志线程将要打印的数据推到队列中，异步线程从队列中拿存储的数据并进行格式化

其中，同步存储部分按照一定转换规则和内存存储将数据存储成fmt的arg store，异步线程通过fmt接口格式化

参考:

https://github.com/MengRao/fmtlog

https://github.com/fmtlib/fmt/pull/2268