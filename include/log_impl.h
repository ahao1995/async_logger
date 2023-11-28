#include "file_helper.h"
#include "fmt_async.h"
#include "log_buffer.h"
#include "log_helper.h"
#include "nanots.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fmt/format.h>
#include <functional>
#include <mutex>
#include <stdint.h>
#include <vector>

namespace async_logger {
struct log_info {
  constexpr log_info(const int level, const int line, const char *fmt,
                     const char *file, const char *func, bool is_kv)
      : level(level), line(line), fmt(fmt), file(file), func(func),
        is_kv(is_kv) {}
  const int level;
  const int line;
  const char *fmt;
  const char *file;
  const char *func;
  bool is_kv;
};

class logger {
public:
  ~logger() = default;
  logger() = default;
  template <typename Context, typename... Args>
  inline void log(const log_info *info, Args &&...args) {
    if (info->level < cur_level_) {
      return;
    }
    uint64_t timestamp = detail::get_current_nano_sec(); // todo: rdtsc
    constexpr size_t num_cstring =
        fmt::detail::count<detail::is_cstring<Context, Args>()...>();
    size_t cstring_sizes[std::max(num_cstring, (size_t)1)];
    size_t s = alloc_size<fmt::format_context>(cstring_sizes, args...) +
               sizeof(addtion_info);
    auto header = alloc(s);
    if (!header) {
      // todo:set callback
      std::cout << "queue full..." << std::endl;
      return;
    }
    char *write_pos = (char *)(header + 1);
    addtion_info add_info{timestamp, info};
    memcpy(write_pos, &add_info, sizeof(addtion_info));
    write_pos += sizeof(addtion_info);
    store((void *)write_pos, info->fmt, cstring_sizes,
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
      addtion_info *info = (addtion_info *)(data);
      data += sizeof(addtion_info);
      auto entry = (async_entry<fmt::format_context> *)(data);
      static const char *log_level_names[] = {"TRACE", "DEBUG", "INFO ",
                                              "WARN ", "ERROR", "FATAL"};
      if (!info->info->is_kv) {
        fmt::format_to(fmt::appender(write_buffer_), "[{}] [{}] {} {}:{} ",
                       tb->get_name(), log_level_names[info->info->level],
                       ts_.convert(info->ts), info->info->file,
                       info->info->line);
      } else {
        fmt::format_to(
            fmt::appender(write_buffer_), "ts={} level={} pid={} file={}:{} ",
            ts_.convert(info->ts), log_level_names[info->info->level],
            tb->get_name(), info->info->file, info->info->line);
      }
      try {
        format_to(*entry, fmt::appender(write_buffer_));
      } catch (...) {
        fmt::format_to(fmt::appender(write_buffer_), "format error");
      }
      fmt::format_to(fmt::appender(write_buffer_), "\n");
      if (file_helper_.is_open()) {
        file_helper_.write(write_buffer_.data(), write_buffer_.size());
        file_helper_.flush();
      } else {
        fwrite(write_buffer_.data(), 1, write_buffer_.size(), stdout);
      }
      write_buffer_.clear();
      tb->pop();
      bg_thread_buffers_[0].header = tb->front();
      adjust_heap(0);
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
  using buffer = fmt::basic_memory_buffer<char, 2048>;
  buffer write_buffer_;
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
