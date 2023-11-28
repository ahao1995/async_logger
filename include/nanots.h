#pragma once
#include <cstdint>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <time.h>

namespace async_logger {
template <size_t SIZE> class Str {
public:
  static const int Size = SIZE;
  char s[SIZE];
  Str() {}
  Str(const char *p) { *this = *(const Str<SIZE> *)p; }

  char &operator[](int i) { return s[i]; }
  char operator[](int i) const { return s[i]; }

  template <typename T> void fromi(T num) {
    if constexpr (Size & 1) {
      s[Size - 1] = '0' + (num % 10);
      num /= 10;
    }
    switch (Size & -2) {
    case 18:
      *(uint16_t *)(s + 16) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 16:
      *(uint16_t *)(s + 14) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 14:
      *(uint16_t *)(s + 12) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 12:
      *(uint16_t *)(s + 10) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 10:
      *(uint16_t *)(s + 8) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 8:
      *(uint16_t *)(s + 6) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 6:
      *(uint16_t *)(s + 4) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 4:
      *(uint16_t *)(s + 2) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    case 2:
      *(uint16_t *)(s + 0) = *(uint16_t *)(digit_pairs + ((num % 100) << 1));
      num /= 100;
    }
  }

  static constexpr const char *digit_pairs = "00010203040506070809"
                                             "10111213141516171819"
                                             "20212223242526272829"
                                             "30313233343536373839"
                                             "40414243444546474849"
                                             "50515253545556575859"
                                             "60616263646566676869"
                                             "70717273747576777879"
                                             "80818283848586878889"
                                             "90919293949596979899";
};

struct time_str {
  Str<3> weekday_name;
  Str<3> month_name;
  Str<4> year;
  char dash1 = '-';
  Str<2> month;
  char dash2 = '-';
  Str<2> day;
  char space = 'T';
  Str<2> hour;
  char colon1 = ':';
  Str<2> minute;
  char colon2 = ':';
  Str<2> second;
  char dot1 = '.';
  Str<9> nanosecond;
};

class nanots {
public:
  nanots() { reset_date(); }
  static int64_t get_current_nano_sec() {
    timespec ts;
    ::clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
  }
  // YYYY-MM-DDTHH:MM:SS.Nanosec
  std::string_view convert(int64_t nano_ts) {
    uint64_t t = (nano_ts > midnight_ns_) ? (nano_ts - midnight_ns_) : 0;
    time_buf_.nanosecond.fromi(t % 1000000000);
    t /= 1000000000;
    time_buf_.second.fromi(t % 60);
    t /= 60;
    time_buf_.minute.fromi(t % 60);
    t /= 60;
    uint32_t h = t;
    if (h > 23) {
      h %= 24;
      reset_date();
    }
    time_buf_.hour.fromi(h);
    return std::string_view(time_buf_.year.s, 29);
  }

private:
  void reset_date() {
    time_t rawtime = get_current_nano_sec() / 1e9;
    struct tm *timeinfo = localtime(&rawtime);
    timeinfo->tm_sec = timeinfo->tm_min = timeinfo->tm_hour = 0;
    midnight_ns_ = mktime(timeinfo) * 1e9;
    time_buf_.year.fromi(1900 + timeinfo->tm_year);
    time_buf_.month.fromi(1 + timeinfo->tm_mon);
    time_buf_.day.fromi(timeinfo->tm_mday);
    const char *weekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    time_buf_.weekday_name = weekdays[timeinfo->tm_wday];
    const char *month_names[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    time_buf_.month_name = month_names[timeinfo->tm_mon];
  }

private:
  time_str time_buf_;
  int64_t midnight_ns_;
};
} // namespace async_logger