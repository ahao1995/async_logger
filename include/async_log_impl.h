#include "async_log_macro.h"
#include "file_helper.h"
#include "fmt/core.h"
#include "fmt_async.h"
#include "log_buffer.h"
#include "log_helper.h"
#include "nanots.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fmt/format.h>
#include <functional>
#include <mutex>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace async_logger {
struct log_info {
  constexpr log_info(const int level, const int line,
                     const std::string_view fmt, const std::string_view file,
                     const std::string_view func, bool is_kv)
      : level(level), line(line), fmt(fmt), file(file), func(func),
        is_kv(is_kv), pos(file.rfind('/')) {}
  const int level;
  const int line;
  const std::string_view fmt;
  const std::string_view file;
  const std::string_view func;
  const bool is_kv;
  const size_t pos;
};

class logger {
public:
  logger() {
    header_args_.reserve(4096);
    header_args_.resize(5);
    set_arg<0>(fmt::string_view());
    set_arg<1>(fmt::string_view());
    set_arg<2>(fmt::string_view());
    set_arg<3>(fmt::string_view());
    set_arg<4>(1);
  }

  ~logger() {
    if (write_buffer_.size()) {
      flush_log_file();
    }
    file_helper_.close();
  }
  template <typename Context, typename... Args>
  inline void log(const log_info *info, Args &&...args) {
    if (info->level < cur_level_) {
      return;
    }
    // todo: rdtsc
    uint64_t timestamp = detail::get_current_nano_sec();
    constexpr size_t num_cstring =
        fmt::detail::count<detail::is_cstring<Context, Args>()...>();
    size_t cstring_sizes[std::max(num_cstring, (size_t)1)];
    size_t s = alloc_size_with_cstring_size<fmt::format_context>(cstring_sizes,
                                                                 args...) +
               sizeof(addtion_info);
    auto header = alloc(s);
    if (!header) {
      // todo:set callback
      fprintf(stderr, "queue_full...\n");
      return;
    }
    char *write_pos = (char *)(header + 1);
    addtion_info add_info{timestamp, info};
    memcpy(write_pos, &add_info, sizeof(addtion_info));
    write_pos += sizeof(addtion_info);
    store_with_cstring_size((void *)write_pos, info->fmt, cstring_sizes,
                            std::forward<Args>(args)...);

    header->push(s);
  }
  void set_log_file(const char *filename) { file_helper_.open(filename); }
  void set_log_level(int8_t logLevel) { cur_level_ = logLevel; }
  int8_t get_log_level() { return cur_level_; }
  void poll() { poll_inner(); }
  void flush() { file_helper_.flush(); }
  void pre_alloc() { ensure_log_buffer_allocated(); }

private:
  struct addtion_info {
    addtion_info(uint64_t ts, const log_info *info) : ts(ts), info(info) {}
    uint64_t ts;
    const log_info *info;
  };
  void ensure_log_buffer_allocated() {
    if (log_buffer_ == nullptr) {
      std::unique_lock<std::mutex> guard(buffer_mutex_);
      guard.unlock();
      log_buffer_ = new log_buffer();
      sbc_.log_buffer_created();
      guard.lock();
      thread_buffers_.push_back(log_buffer_);
    }
  }
  log_buffer::queue_header *alloc(size_t size) {
    if (log_buffer_ == nullptr) {
      ensure_log_buffer_allocated();
    }
    return log_buffer_->alloc(size);
  }
  template <size_t I, typename T> void set_arg(const T &arg) {
    header_args_[I] = fmt::detail::make_arg<fmt::format_context>(arg);
  }
  template <size_t I, typename T> void set_arg_val(const T &arg) {
    fmt::detail::value<fmt::format_context> &value_ =
        *(fmt::detail::value<fmt::format_context> *)&header_args_[I];
    value_ = fmt::detail::arg_mapper<fmt::format_context>().map(arg);
  }
  void flush_log_file() {
    if (file_helper_.is_open()) {
      file_helper_.write(write_buffer_.data(), write_buffer_.size());
      file_helper_.flush();
    }
    write_buffer_.clear();
  }

  void handle_log(fmt::string_view thread_name, const char *data) {
    addtion_info *info = (addtion_info *)(data);
    data += sizeof(addtion_info);
    auto entry = (async_entry<fmt::format_context> *)(data);
    static const std::array<fmt::string_view, 6> log_level_names{
        "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};
    set_arg_val<0>(thread_name);
    set_arg_val<1>(log_level_names[info->info->level]);
    set_arg_val<2>(ts_.convert(info->ts));
    set_arg_val<3>(info->info->file.substr(info->info->pos + 1));
    set_arg_val<4>(info->info->line);
    if (!info->info->is_kv) {
      static fmt::string_view header_pattern{"[{}] [{}] {} {}:{} "};
      fmt::detail::vformat_to(write_buffer_, header_pattern,
                              fmt::basic_format_args(header_args_.data(), 5));
    } else {
      static fmt::string_view kv_header_pattern{
          "ts={} level={} pid={} file={}:{} "};
      fmt::detail::vformat_to(write_buffer_, kv_header_pattern,
                              fmt::basic_format_args(header_args_.data(), 5));
    }
    try {
      format_to(*entry, fmt::appender(write_buffer_));
    } catch (...) {
      fmt::format_to(fmt::appender(write_buffer_), "format error");
    }
    write_buffer_.push_back('\n');
    if (file_helper_.is_open()) {
      if (write_buffer_.size() >= flush_buf_size_) {
        flush_log_file();
      }
    } else {
      fwrite(write_buffer_.data(), 1, write_buffer_.size(), stdout);
    }
  }

  void poll_inner() {
    uint64_t ts = detail::get_current_nano_sec();
    if (thread_buffers_.size()) {
      std::unique_lock<std::mutex> lock(buffer_mutex_);
      for (auto tb : thread_buffers_) {
        bg_thread_buffers_.emplace_back(tb);
      }
      thread_buffers_.clear();
    }
    for (size_t i = 0; i < bg_thread_buffers_.size(); i++) {
      auto &node = bg_thread_buffers_[i];
      if (node.header)
        continue;
      node.header = node.tb->front();
      if (!node.header && node.tb->check_can_delete()) {
        delete node.tb;
        node = bg_thread_buffers_.back();
        bg_thread_buffers_.pop_back();
        i--;
      }
    }
    if (bg_thread_buffers_.empty())
      return;

    // build heap
    for (int i = bg_thread_buffers_.size() / 2; i >= 0; i--) {
      adjust_heap(i);
    }
    while (true) {
      auto header = bg_thread_buffers_[0].header;
      if (!header || *(uint64_t *)(header + 1) >= ts)
        break;
      auto tb = bg_thread_buffers_[0].tb;
      const char *data = (const char *)(header + 1);
      handle_log(tb->get_name(), data);
      tb->pop();
      bg_thread_buffers_[0].header = tb->front();
      adjust_heap(0);
    }
    flush_log_file();
  }

  void adjust_heap(size_t i) {
    while (true) {
      size_t min_i = i;
      size_t ch = std::min(i * 2 + 1, bg_thread_buffers_.size());
      size_t end = std::min(ch + 2, bg_thread_buffers_.size());
      for (; ch < end; ch++) {
        auto h_ch = bg_thread_buffers_[ch].header;
        auto h_min = bg_thread_buffers_[min_i].header;
        if (h_ch &&
            (!h_min || *(int64_t *)(h_ch + 1) < *(int64_t *)(h_min + 1)))
          min_i = ch;
      }
      if (min_i == i)
        break;
      std::swap(bg_thread_buffers_[i], bg_thread_buffers_[min_i]);
      i = min_i;
    }
  }

private:
  class log_buffer_destroyer {
  public:
    explicit log_buffer_destroyer() {}
    void log_buffer_created() {}
    virtual ~log_buffer_destroyer() {
      if (logger::log_buffer_ != nullptr) {
        log_buffer_->set_delete_flag();
        logger::log_buffer_ = nullptr;
      }
    }
    friend class async_logger;
  };

private:
  struct heap_node {
    heap_node(log_buffer *buffer) : tb(buffer) {}
    log_buffer *tb;
    const log_buffer::queue_header *header{nullptr};
  };
  std::vector<heap_node> bg_thread_buffers_;

private:
  int8_t cur_level_{0};
  file_helper file_helper_;
  nanots ts_;

  inline static thread_local log_buffer *log_buffer_{nullptr};
  inline static thread_local log_buffer_destroyer sbc_{};
  std::vector<log_buffer *> thread_buffers_;
  std::mutex buffer_mutex_;
  using buffer = fmt::basic_memory_buffer<char, 10000>;
  buffer write_buffer_;
  uint32_t flush_buf_size_{8 * 1024};
  std::vector<fmt::basic_format_arg<fmt::format_context>> header_args_;
};

} // namespace async_logger

static async_logger::logger log_instance;

#define ASYNC_LOG_KV_INNER(logger, level, ...)                                 \
  do {                                                                         \
    static constexpr auto fmt = async_logger::detail::gen_fmt(                 \
        []() {                                                                 \
          return MAKE_FMT_ARRAY(PP_GET_ARG_COUNT(__VA_ARGS__), __VA_ARGS__);   \
        },                                                                     \
        MAKE_LOG_ARGS(PP_GET_ARG_COUNT(__VA_ARGS__), __VA_ARGS__));            \
    static constexpr async_logger::log_info i{                                 \
        level, __LINE__, fmt.data(), __FILE__, __FUNCTION__, true};            \
    logger.log<fmt::format_context>(                                           \
        &i, MAKE_LOG_ARGS(PP_GET_ARG_COUNT(__VA_ARGS__), __VA_ARGS__));        \
  } while (0)
