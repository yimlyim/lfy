// Represents the output medium for log messages.
// This could be a file, console, or any other output, such as a network
// socket/memory buffers etc.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <mutex>
#include <print>
#include <vector>

namespace lfy {

namespace details {

inline void safe_fclose(FILE *f) {
  if (f != nullptr)
    std::fclose(f);
}
} // namespace details

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
  virtual std::chrono::system_clock::time_point lastFlush() = 0;
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
    if (m_buffer.size() > 0)
      flush();
  }

  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};

    // If the buffer is full, flush it before adding new data
    if (m_buffer.size() + message.size() + 1 > m_buffer.capacity())
      flush();

    // Big messages which exceed the buffer size, are written directly to avoid
    // repeated flushes
    if (message.size() + 1 > m_buffer.capacity()) {
      std::fwrite(message.data(), sizeof(char), message.size(), stdout);
      return;
    }

    m_buffer.insert(m_buffer.end(), message.begin(), message.end());
    m_buffer.insert(m_buffer.end(), '\n');
  }

  std::chrono::system_clock::time_point lastFlush() override {
    std::lock_guard l{m_mutex};
    return m_lastFlush;
  }

  void flush() override {
    std::lock_guard l{m_mutex};

    std::fwrite(m_buffer.data(), sizeof(char), m_buffer.size(), stdout);
    m_buffer.clear();

    m_lastFlush = std::chrono::system_clock::now();
  }

private:
  std::recursive_mutex m_mutex;
  std::vector<char> m_buffer;
  std::chrono::system_clock::time_point m_lastFlush{
      std::chrono::system_clock::now()};
};

class FileOutputter : public Outputter {
public:
  explicit FileOutputter(const std::filesystem::path filePath,
                         std::size_t bufferSize = 64 * literals::KiB)
      : m_filePath{std::move(filePath)},
        m_file{nullptr, &details::safe_fclose} {
    init(bufferSize);
  }

  FileOutputter(std::size_t bufferSize, const std::filesystem::path filePath)
      : m_filePath{std::move(filePath)},
        m_file{nullptr, &details::safe_fclose} {
    init(bufferSize);
  }

  ~FileOutputter() {
    std::lock_guard l{m_mutex};
    // Flush any remaining data in the buffer before destruction
    if (m_buffer.size() > 0)
      flush();
  }
  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};

    // If the buffer is full, flush it before adding new data
    if (m_buffer.size() + message.size() + 1 > m_buffer.capacity())
      flush();

    // Big messages which exceed the buffer size, are written directly to avoid
    // repeated flushes
    if (message.size() + 1 > m_buffer.capacity()) {
      std::fwrite(message.data(), sizeof(char), message.size(), m_file.get());
      return;
    }

    m_buffer.insert(m_buffer.end(), message.begin(), message.end());
    m_buffer.insert(m_buffer.end(), '\n');
  }

  std::chrono::system_clock::time_point lastFlush() override {
    std::lock_guard l{m_mutex};
    return m_lastFlush;
  }

  void flush() override {
    std::lock_guard l{m_mutex};

    std::fwrite(m_buffer.data(), sizeof(char), m_buffer.size(), m_file.get());
    m_buffer.clear();

    m_lastFlush = std::chrono::system_clock::now();
  }

private:
  void init(std::size_t bufferSize) {
    if (!std::filesystem::is_regular_file(m_filePath))
      throw std::runtime_error("FileOutputter: File" + m_filePath.string() +
                               "is not a regular file.");

    m_file = std::unique_ptr<FILE, decltype(&details::safe_fclose)>(
        std::fopen(m_filePath.string().data(), "a"), &details::safe_fclose);

    if (m_file == nullptr)
      throw std::runtime_error("FileOutputter: Failed to open file " +
                               m_filePath.string());

    m_buffer.reserve(bufferSize);
    if (setvbuf(m_file.get(), nullptr, _IONBF, 0) != 0)
      throw std::runtime_error("FileOutputter: Failed to set buffer mode for " +
                               m_filePath.string());
  }

  std::recursive_mutex m_mutex;
  std::filesystem::path m_filePath;
  std::vector<char> m_buffer;
  std::unique_ptr<FILE, decltype(&details::safe_fclose)> m_file;
  std::chrono::system_clock::time_point m_lastFlush{
      std::chrono::system_clock::now()};
};

class MemoryOutputter : public Outputter {
  // Todo, implement MemoryOutputter
  // Either own a memory buffer or use a shared buffer
  // If own buffer, think about how to allow exposure of the buffer to
  // consumers without breaking encapsulation.
};

namespace outputters {

template <typename... Args> inline auto Console(Args &&...args) {
  return std::make_shared<ConsoleOutputter>(std::forward<Args>(args)...);
}

template <typename... Args> inline auto File(Args &&...args) {
  return std::make_shared<FileOutputter>(std::forward<Args>(args)...);
};

} // namespace outputters

} // namespace lfy