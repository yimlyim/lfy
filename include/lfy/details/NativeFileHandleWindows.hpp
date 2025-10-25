// Windows native file handle implementation (header-only)
#pragma once

#ifndef _WIN32
#error "NativeFileHandleWindows included on non-Windows platform"
#endif

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace lfy::details {

struct NativeFile {
  HANDLE handle{INVALID_HANDLE_VALUE};
};

inline NativeFile open_for_append(const std::filesystem::path &p) {
  NativeFile nf{};
  nf.handle =
      ::CreateFileW(p.wstring().c_str(), FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  return nf;
}

inline bool valid(const NativeFile &nf) {
  return nf.handle != INVALID_HANDLE_VALUE;
}

inline void close_native(NativeFile &nf) {
  if (nf.handle != INVALID_HANDLE_VALUE) {
    ::CloseHandle(nf.handle);
    nf.handle = INVALID_HANDLE_VALUE;
  }
}

inline void write_bytes(NativeFile &nf, const char *data, std::size_t len) {
  std::size_t total = 0;
  while (total < len) {
    DWORD written = 0;
    DWORD toWrite = static_cast<DWORD>(std::min<std::size_t>(
        len - total, static_cast<std::size_t>(UINT32_MAX)));
    if (!::WriteFile(nf.handle, data + total, toWrite, &written, nullptr)) {
      throw std::runtime_error("FileOutputter: WriteFile failed");
    }
    if (written == 0) {
      throw std::runtime_error("FileOutputter: WriteFile wrote 0 bytes");
    }
    total += written;
  }
}

// Atomic append of message + '\n'. Build contiguous buffer safely.
inline void write_line(NativeFile &nf, const char *data, std::size_t len) {
  std::vector<char> tmp;
  tmp.resize(len + 1);
  if (len)
    std::memcpy(tmp.data(), data, len);
  tmp[len] = '\n';
  write_bytes(nf, tmp.data(), tmp.size());
}

} // namespace lfy::details
