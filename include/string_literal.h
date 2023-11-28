#define pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace async_logger {
template <std::size_t N> struct string_literal {
  std::array<char, N + 1> elements_{};

  constexpr string_literal() noexcept : elements_{} {}

  constexpr string_literal(const char (&data)[N + 1]) noexcept {
    for (size_t i = 0; i < N + 1; i++)
      elements_[i] = data[i];
  }

  constexpr string_literal(std::string_view s) {
    for (size_t i = 0; i < N; ++i) {
      elements_[i] = s[i];
    }
  }

  constexpr char &operator[](std::size_t index) noexcept {
    return elements_[index];
  }

  constexpr const char &operator[](std::size_t index) const noexcept {
    return elements_[index];
  }

  constexpr operator std::string_view() const noexcept {
    return std::string_view{elements_.data(), size()};
  }

  constexpr bool empty() const noexcept { return size() == 0; }

  constexpr std::size_t size() const noexcept { return N; }

  constexpr char &front() noexcept { return elements_.front(); }

  constexpr const char &front() const noexcept { return elements_.front(); }

  constexpr char &back() noexcept { return elements_[size() - 1]; }

  constexpr const char &back() const noexcept { return elements_[size() - 1]; }

  constexpr auto begin() noexcept { return elements_.begin(); }

  constexpr auto begin() const noexcept { return elements_.begin(); }

  constexpr auto end() noexcept { return elements_.begin() + size(); }

  constexpr auto end() const noexcept { return elements_.begin() + size(); }

  constexpr char *data() noexcept { return elements_.data(); }

  constexpr const char *data() const noexcept { return elements_.data(); };

  constexpr const char *c_str() const noexcept { return elements_.data(); }

  constexpr bool contains(char c) const noexcept {
    return std::find(begin(), end(), c) != end();
  }

  constexpr bool contains(std::string_view str) const noexcept {
    return str.size() <= size()
               ? std::search(begin(), end(), str.begin(), str.end()) != end()
               : false;
  }

  static constexpr size_t substr_len(size_t pos, size_t count) {
    if (pos >= N) {
      return 0;
    } else if (count == std::string_view::npos || pos + count > N) {
      return N - pos;
    } else {
      return count;
    }
  }

  template <size_t pos, size_t count = std::string_view::npos>
  constexpr string_literal<substr_len(pos, count)> substr() const noexcept {
    constexpr size_t n = substr_len(pos, count);

    string_literal<n> result;
    for (std::size_t i = 0; i < n; ++i) {
      result[i] = elements_[pos + i];
    }
    return result;
  }

  constexpr size_t rfind(char c) const noexcept {
    return std::string_view(*this).rfind(c);
  }

  constexpr size_t find(char c) const noexcept {
    return std::string_view(*this).find(c);
  }
};

template <std::size_t N>
string_literal(const char (&)[N]) -> string_literal<N - 1>;

template <std::size_t M, std::size_t N>
constexpr bool operator==(const string_literal<M> &left,
                          const string_literal<N> &right) noexcept {
  return static_cast<std::string_view>(left) ==
         static_cast<std::string_view>(right);
}

template <std::size_t M, std::size_t N>
constexpr bool operator==(const string_literal<M> &left,
                          const char (&right)[N]) noexcept {
  return static_cast<std::string_view>(left) ==
         static_cast<std::string_view>(string_literal{right});
}

template <std::size_t M, std::size_t N>
constexpr auto operator+(const string_literal<M> &left,
                         const string_literal<N> &right) noexcept {
  string_literal<M + N> s;
  for (size_t i = 0; i < M; ++i)
    s[i] = left[i];
  for (size_t i = 0; i < N; ++i)
    s[M + i] = right[i];
  return s;
}

template <std::size_t M, std::size_t N>
constexpr auto operator+(const string_literal<M> &left,
                         const char (&right)[N]) noexcept {
  string_literal<M + N - 1> s;
  for (size_t i = 0; i < M; ++i)
    s[i] = left[i];
  for (size_t i = 0; i < N; ++i)
    s[M + i] = right[i];
  return s;
}

template <std::size_t M, std::size_t N>
constexpr auto operator+(const char (&left)[M],
                         const string_literal<N> &right) noexcept {
  string_literal<M + N - 1> s;
  for (size_t i = 0; i < M - 1; ++i)
    s[i] = left[i];
  for (size_t i = 0; i < N; ++i)
    s[M + i - 1] = right[i];
  return s;
}
} // namespace async_logger
