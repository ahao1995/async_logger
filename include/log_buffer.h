#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef LOG_QUEUE_SIZE
#define LOG_QUEUE_SIZE (1 << 20)
#endif
namespace async_logger {
class log_queue {
public:
  struct msg_header {
    inline void push(uint32_t sz) {
      *(volatile uint32_t *)&size = sz + sizeof(msg_header);
    }

    uint32_t size;
    uint32_t logId;
    uint32_t reserve1;
    uint32_t reserve2;
  };
  static constexpr uint32_t BLK_CNT = 4 * LOG_QUEUE_SIZE / sizeof(msg_header);

  msg_header *allocMsg(uint32_t size) noexcept;

  msg_header *alloc(uint32_t size) {
    size += sizeof(msg_header);
    uint32_t blk_sz = (size + sizeof(msg_header) - 1) / sizeof(msg_header);
    if (blk_sz >= free_write_cnt) {
      uint32_t read_idx_cache = *(volatile uint32_t *)&read_idx;
      if (read_idx_cache <= write_idx) {
        free_write_cnt = BLK_CNT - write_idx;
        if (blk_sz >= free_write_cnt && read_idx_cache != 0) { // wrap around
          blk[0].size = 0;
          blk[write_idx].size = 1;
          write_idx = 0;
          free_write_cnt = read_idx_cache;
        }
      } else {
        free_write_cnt = read_idx_cache - write_idx;
      }
      if (free_write_cnt <= blk_sz) {
        return nullptr;
      }
    }
    msg_header *ret = &blk[write_idx];
    write_idx += blk_sz;
    free_write_cnt -= blk_sz;
    blk[write_idx].size = 0;
    return ret;
  }

  inline const msg_header *front() {
    uint32_t size = blk[read_idx].size;
    if (size == 1) { // wrap around
      read_idx = 0;
      size = blk[0].size;
    }
    if (size == 0)
      return nullptr;
    return &blk[read_idx];
  }

  inline void pop() {
    uint32_t blk_sz =
        (blk[read_idx].size + sizeof(msg_header) - 1) / sizeof(msg_header);
    *(volatile uint32_t *)&read_idx = read_idx + blk_sz;
  }

private:
  alignas(64) msg_header blk[BLK_CNT] = {};
  uint32_t write_idx = 0;
  uint32_t free_write_cnt = BLK_CNT;

  alignas(128) uint32_t read_idx = 0;
};

class log_buffer {
public:
  using queue_header = log_queue::msg_header;
  log_buffer() : varq_() {
    uint32_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    snprintf(name_, sizeof(name_), "%d", tid);
  }
  ~log_buffer() {}
  inline queue_header *alloc(size_t nbytes) { return varq_.alloc(nbytes); }

  const queue_header *front() { return varq_.front(); }
  inline void pop() { varq_.pop(); }
  void set_name(const char *name) { strncpy(name_, name, sizeof(name) - 1); }
  const char *get_name() { return name_; }

  bool check_can_delete() { return should_deallocate_; }

  void set_delete_flag() { should_deallocate_ = true; }

private:
  log_queue varq_;
  char name_[16] = {0};
  bool should_deallocate_{false};
};
} // namespace async_logger