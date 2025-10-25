// POSIX native file handle implementation (header-only)
#pragma once

#if defined(_WIN32)
#error "NativeFileHandleLinux included on Windows platform"
#endif

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

namespace lfy::details {

struct NativeFile {
  int fd{-1};
};

inline NativeFile open_for_append(const std::filesystem::path &p) {
  NativeFile nf{};
  nf.fd = ::open(p.string().c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                 0644);
  return nf;
}

inline bool valid(const NativeFile &nf) { return nf.fd != -1; }

inline void close_native(NativeFile &nf) {
  if (nf.fd != -1) {
    ::close(nf.fd);
    nf.fd = -1;
  }
}

inline void write_bytes(NativeFile &nf, const char *data, std::size_t len) {
  std::size_t total = 0;
  while (total < len) {
    ssize_t written = ::write(nf.fd, data + total, len - total);
    if (written < 0) {
      if (errno == EINTR)
        continue;
      throw std::runtime_error("FileOutputter: write failed");
    }
    if (written == 0)
      throw std::runtime_error("FileOutputter: write returned 0");
    total += static_cast<std::size_t>(written);
  }
}

inline void write_line(NativeFile &nf, const char *data, std::size_t len) {
  struct iovec vec[2]{{const_cast<char *>(data), len},
                      {const_cast<char *>("\n"), 1}};
  std::size_t expected = len + 1;
  ssize_t written = ::writev(nf.fd, vec, 2);
  if (written < 0) {
    if (errno == EINTR) {
      // Retry EINTR via bytes path fallback.
      write_bytes(nf, data, len);
      write_bytes(nf, "\n", 1);
      return;
    }
    throw std::runtime_error("FileOutputter: writev failed");
  }
  if (static_cast<std::size_t>(written) != expected) {
    // Fallback partial atomic writev scenario.
    const std::size_t remaining = expected - static_cast<std::size_t>(written);
    // If only newline left
    if (remaining == 1) {
      write_bytes(nf, "\n", 1);
    } else {
      // Remaining part of message + newline; simplest fallback re-write
      // remainder.
      const char *start = data + (expected - remaining - 1);
      write_bytes(nf, start, remaining - 1);
      write_bytes(nf, "\n", 1);
    }
  }
}

} // namespace lfy::details
