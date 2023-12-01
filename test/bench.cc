#include "async_log.h"
struct log_base {
  void flush() { POLL(); }
};
struct static_string : public log_base {
  inline void log() {
    ASYNC_LOG(LOG_DEBUG, "Starting backup replica garbage collector thread");
  }
};

struct string_concat : public log_base {
  inline void log() {
    ASYNC_LOG(LOG_DEBUG, "Opened session with coordinator at {}",
              "basic+udp:host=192.168.1.140,port=12246");
  }
};

struct single_integer : public log_base {
  inline void log() {
    ASYNC_LOG(LOG_DEBUG, "Backup storage speeds (min): {} MB/s read", 181);
  }
};

struct two_integers : public log_base {
  inline void log() {
    ASYNC_LOG(
        LOG_DEBUG,
        "buffer has consumed {} bytes of extra storage, current allocation: "
        "{} bytes",
        1032024, 1016544);
  }
};

struct single_double : public log_base {
  inline void log() {
    ASYNC_LOG(LOG_DEBUG, "Using tombstone ratio balancer with ratio = {}",
              0.400000);
  }
};

struct complex_format : public log_base {
  inline void log() {
    ASYNC_LOG(LOG_DEBUG,
              "Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} "
              "transmit buffers ({} MB), took {:.1f} ms",
              50000, 97, 50, 0, 26.2);
  }
};

template <typename T> void bench(T o) {
  const int RECORDS = 10000;
  std::chrono::high_resolution_clock::time_point t0, t1, t2;
  t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < RECORDS; ++i) {
    o.log();
  }
  t1 = std::chrono::high_resolution_clock::now();
  o.flush();
  t2 = std::chrono::high_resolution_clock::now();
  double span1 =
      std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0)
          .count();
  double span2 =
      std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t0)
          .count();
  fmt::print("{}: front-end latency is {:.1f} ns/msg average, throughput  is "
             "{:.2f} million msgs/sec average\n",
             typeid(o).name(), (span1 / RECORDS) * 1e9, RECORDS / span2 / 1e6);
}

int main() {
  SET_LOG_FILE("bench.log");
  PRE_ALLOC();
  bench(static_string());
  bench(string_concat());
  bench(single_integer());
  bench(two_integers());
  bench(single_double());
  bench(complex_format());
}
