// Represents the output medium for log messages.
// This could be a file, console, or any other output, such as a network
// socket/memory buffers etc.

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "details/NativeFileHandleWrapper.hpp"

namespace lfy {

template <std::size_t N> struct BufferCapacity {};

// Platform-specific NativeFile and helper functions live in detail headers.
namespace details {} // namespace details

namespace literals {

constexpr std::size_t operator""_KiB(unsigned long long x) { return x << 10; }
constexpr std::size_t operator""_MiB(unsigned long long x) { return x << 20; }
constexpr std::size_t operator""_GiB(unsigned long long x) { return x << 30; }

constexpr std::size_t KiB = 1_KiB;
constexpr std::size_t MiB = 1_MiB;
constexpr std::size_t GiB = 1_GiB;

} // namespace literals

class Outputter {
public:
  virtual ~Outputter() = default;
  virtual void output(const std::string &message) = 0;
  virtual std::chrono::steady_clock::time_point lastFlush() = 0;
  virtual void flush() = 0;
};

// ConsoleOutputter is fully buffered and flushes standard output.
// Messages are always either fully buffered or written directly to avoid
// partial messages between flushes.
class ConsoleOutputter : public Outputter {
public:
  ConsoleOutputter(std::size_t bufferSize = 4 * literals::KiB) {
    m_buffer.reserve(bufferSize);
  }
  ~ConsoleOutputter() {
    std::lock_guard l{m_mutex};
    // Flush any remaining data in the buffer before destruction
    if (m_buffer.empty())
      return;
    std::fwrite(m_buffer.data(), 1, m_buffer.size(), stdout);
    std::fflush(stdout);
  }

  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};

    // If the buffer is full, flush it before adding new data
    if (m_buffer.size() + message.size() + 1 > m_buffer.capacity())
      flushUnlocked();

    // Big messages which exceed the buffer size, are written directly to
    // avoid repeated flushes
    if (message.size() + 1 > m_buffer.capacity()) {
      std::fwrite(message.data(), 1, message.size(), stdout);
      std::fwrite("\n", 1, 1, stdout);
      std::fflush(stdout);
      m_lastFlush = std::chrono::steady_clock::now();
      return;
    }

    m_buffer.insert(m_buffer.end(), message.begin(), message.end());
    m_buffer.insert(m_buffer.end(), '\n');
  }

  std::chrono::steady_clock::time_point lastFlush() override {
    std::lock_guard l{m_mutex};
    return m_lastFlush;
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    flushUnlocked();
  }

private:
  void flushUnlocked() {
    if (m_buffer.empty())
      return;
    std::fwrite(m_buffer.data(), 1, m_buffer.size(), stdout);
    std::fflush(stdout);
    m_buffer.clear();
    m_lastFlush = std::chrono::steady_clock::now();
  }

  std::mutex m_mutex;
  std::vector<char> m_buffer;
  std::chrono::steady_clock::time_point m_lastFlush{
      std::chrono::steady_clock::now()};
};

template <size_t N> class FileOutputter : public Outputter {
public:
  FileOutputter(std::filesystem::path filePath)
      : m_filePath{std::move(filePath)} {
    init();
  }

  ~FileOutputter() override {
    std::lock_guard l{m_mutex};
    // Flush any remaining data in the buffer before destruction
    if (m_bufferWriteIndex == 0)
      return;
    details::write_bytes(m_file, m_buffer.data(), m_bufferWriteIndex);
    details::close_native(m_file);
  }
  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};

    // If the buffer is full, flush it before adding new data
    if ((m_bufferWriteIndex + message.size() + 1) > m_buffer.size())
      flushUnlocked();

    // Big messages which exceed the buffer size, are written directly to
    // avoid repeated flushes
    // Todo possibly optimize away two fwrite calls
    if (message.size() + 1 > m_buffer.size()) {
      // Atomic append of message + newline without copying entire buffer on
      // POSIX; Windows builds temp
      details::write_line(m_file, message.data(), message.size());
      m_lastFlush = std::chrono::steady_clock::now();
      m_bufferWriteIndex = 0;
      return;
    }

    std::memcpy(&m_buffer[m_bufferWriteIndex], message.data(), message.size());
    m_bufferWriteIndex += message.size();
    m_buffer[m_bufferWriteIndex++] = '\n';
  }

  std::chrono::steady_clock::time_point lastFlush() override {
    std::lock_guard l{m_mutex};
    return m_lastFlush;
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    flushUnlocked();
  }

private:
  void flushUnlocked() {
    if (m_bufferWriteIndex == 0)
      return;
    details::write_bytes(m_file, m_buffer.data(), m_bufferWriteIndex);
    m_bufferWriteIndex = 0;
    m_lastFlush = std::chrono::steady_clock::now();
  }

  void init() {
    m_file = details::open_for_append(m_filePath);
    if (!details::valid(m_file)) {
      throw std::runtime_error(
          "FileOutputter: Failed to open file " + m_filePath.string() +
          (std::filesystem::exists(m_filePath) ? " (Not enough access rights)"
                                               : ""));
    }
  }

  std::mutex m_mutex;
  std::filesystem::path m_filePath;
  details::NativeFile m_file;

  std::array<char, N> m_buffer;      // Fixed-size buffer
  std::size_t m_bufferWriteIndex{0}; // Current write index in the buffer

  std::chrono::steady_clock::time_point m_lastFlush{
      std::chrono::steady_clock::now()};
};

template <typename BufferType> class MemoryOutputter : public Outputter {
  // Todo, implement MemoryOutputter
  // Either own a memory buffer or use a shared buffer
  // If own buffer, think about how to allow exposure of the buffer to
  // consumers without breaking encapsulation
public:
  MemoryOutputter(BufferType buffer) : m_buffer(std::move(buffer)) {}

  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};
    m_buffer.append(message);
  }
  void flush() override { return; }

private:
  std::mutex m_mutex;
  BufferType m_buffer;
};

namespace outputters {

template <typename... Args, typename BufferType>
inline auto Memory(Args &&...args, BufferType buffer) {
  return std::make_shared<MemoryOutputter<BufferType>>(
      std::forward<Args>(args)..., std::move(buffer));
}

template <typename... Args> inline auto Console(Args &&...args) {
  return std::make_shared<ConsoleOutputter>(std::forward<Args>(args)...);
}

inline auto File(std::filesystem::path filePath) {
  return std::make_shared<FileOutputter<64 * literals::KiB>>(filePath);
}

template <typename... Args, std::size_t N>
inline auto File(std::filesystem::path filePath, BufferCapacity<N>) {
  return std::make_shared<FileOutputter<N>>(filePath);
}

} // namespace outputters

} // namespace lfy