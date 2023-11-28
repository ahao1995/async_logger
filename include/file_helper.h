#pragma once

#include <cstdint>
#include <string>
#include <sys/stat.h>
namespace async_logger {
class file_helper {
public:
  file_helper() = default;
  file_helper(const file_helper &) = delete;
  file_helper &operator=(const file_helper &) = delete;
  ~file_helper() { close(); }

  bool is_open() { return fd_ != nullptr; }
  void open(const std::string &fname, bool truncate = false) {
    close();
    filename_ = fname;
    create_dir(dir_name(fname));
    if (truncate) {
      // Truncate by opening-and-closing a tmp file in "wb" mode, always
      // opening the actual log-we-write-to in "ab" mode, since that
      // interacts more politely with eternal processes that might
      // rotate/truncate the file underneath us.
      std::FILE *tmp = ::fopen((fname.c_str()), "wb");
      if (tmp == nullptr) {
        return;
      }
      std::fclose(tmp);
    }
    fd_ = ::fopen((fname.c_str()), "ab");
    if (fd_) {
      return;
    }
    printf("open file error\n");
  }
  void reopen(bool truncate) {
    if (filename_.empty()) {
    }
    this->open(filename_, truncate);
  }
  void flush() {
    if (std::fflush(fd_) != 0) {
      printf("Failed flush to file\n");
    }
  }
  void close() {
    if (fd_ != nullptr) {
      std::fclose(fd_);
      fd_ = nullptr;
    }
  }
  void write(const char *buf, size_t len) {
    if (std::fwrite(buf, 1, len, fd_) != len) {
      printf("Failed writing to file\n");
    }
  }
  size_t size() const {
    if (fd_ == nullptr) {
      printf("Cannot use size() on closed file ");
    }
    return filesize(fd_);
  }
  const std::string &filename() const { return filename_; }
  bool path_exists(const std::string &filename) {
    struct stat buffer;
    return (::stat(filename.c_str(), &buffer) == 0);
  }
  bool create_dir(const std::string &path) {
    if (path_exists(path)) {
      return true;
    }

    if (path.empty()) {
      return false;
    }

    size_t search_offset = 0;
    do {
      auto token_pos = path.find_first_of("/", search_offset);
      // treat the entire path as a folder if no folder separator not found
      if (token_pos == std::string::npos) {
        token_pos = path.size();
      }

      auto subdir = path.substr(0, token_pos);
      if (!subdir.empty() && !path_exists(subdir) &&
          !(::mkdir(subdir.c_str(), mode_t(0755)) == 0)) {
        return false;
      }
      search_offset = token_pos + 1;
    } while (search_offset < path.size());

    return true;
  }
  std::string dir_name(const std::string &path) {
    auto pos = path.find_last_of("/");
    return pos != std::string::npos ? path.substr(0, pos) : std::string{};
  }

private:
  size_t filesize(FILE *f) const {
    if (f == nullptr) {
      printf("Failed getting file size. fd is null");
    }
    int fd = ::fileno(f);
    struct stat64 st;
    if (::fstat64(fd, &st) == 0) {
      return static_cast<size_t>(st.st_size);
    }
    printf("Failed getting file size from fd");
    return 0; // will not be reached.
  }

private:
  std::FILE *fd_{nullptr};
  std::string filename_;
};
} // namespace async_logger