#include "log_impl.h"
#include "log_macro.h"

enum {
  LOG_TRACE = 0,
  LOG_DEBUG = 1,
  LOG_INFO = 2,
  LOG_WARN = 3,
  LOG_ERROR = 4,
  LOG_FATAL = 5,
  NUM_LOG_LEVELS
};

#define SET_LOG_FILE(file)                                                     \
  do {                                                                         \
    log_instance.set_log_file(file);                                           \
  } while (0)

#define SET_LOG_LEVEL(level)                                                   \
  do {                                                                         \
    log_instance.set_log_level(level);                                         \
  } while (0)

#define ASYNC_LOG(level, format, ...)                                          \
  do {                                                                         \
    static constexpr async_logger::log_info async_logger_log_info{             \
        level, __LINE__, format, __FILE__, __FUNCTION__, false};               \
    log_instance.log<fmt::format_context>(&async_logger_log_info,              \
                                          ##__VA_ARGS__);                      \
  } while (0)

#define ASYNC_LOG_KV(level, event, ...)                                        \
  ASYNC_LOG_KV_INNER(log_instance, level, ("event", event), __VA_ARGS__)

#define PRE_ALLOC()                                                            \
  do {                                                                         \
    log_instance.pre_alloc();                                                  \
  } while (0)

#define POLL()                                                                 \
  do {                                                                         \
    log_instance.poll();                                                       \
  } while (0)
