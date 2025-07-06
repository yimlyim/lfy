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

class ConsoleOutputter : public Outputter {
public:
  ConsoleOutputter(std::size_t bufferSize = 4 * literals::KiB) {
    m_buffer.reserve(bufferSize);
    std::cout.rdbuf()->pubsetbuf(m_buffer.data(), m_buffer.capacity());
  }
  ~ConsoleOutputter() = default;

  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};
    std::println("{}", message);
  }

  std::chrono::system_clock::time_point lastFlush() override {
    std::lock_guard l{m_mutex};
    return m_lastFlush;
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    std::cout.flush();
    m_lastFlush = std::chrono::system_clock::now();
  }

private:
  std::mutex m_mutex;
  std::vector<char> m_buffer;
  std::chrono::system_clock::time_point m_lastFlush{
      std::chrono::system_clock::now()};
};

class FileOutputter : public Outputter {
public:
  explicit FileOutputter(const std::filesystem::path filePath,
                         std::size_t bufferSize = 16 * literals::KiB)
      : m_filePath{std::move(filePath)} {
    init(bufferSize);
  }

  FileOutputter(std::size_t bufferSize, const std::filesystem::path filePath)
      : m_filePath{std::move(filePath)} {
    init(bufferSize);
  }

  ~FileOutputter() = default;
  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};

    std::println(m_filestream, "{}", message);
  }

  std::chrono::system_clock::time_point lastFlush() override {
    std::lock_guard l{m_mutex};
    return m_lastFlush;
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    if (!m_filestream.is_open())
      return;

    m_filestream.flush();
    m_lastFlush = std::chrono::system_clock::now();
  }

private:
  void init(std::size_t bufferSize) {
    if (std::filesystem::exists(m_filePath) &&
        !std::filesystem::is_regular_file(m_filePath))
      throw std::runtime_error("FileOutputter: File" + m_filePath.string() +
                               " does not exist or is not a regular file.");
    m_filestream.open(m_filePath, std::ios::out | std::ios::app);
    if (!m_filestream.is_open())
      throw std::runtime_error("FileOutputter: Failed to open file " +
                               m_filePath.string());

    m_buffer.reserve(bufferSize);
    m_filestream.rdbuf()->pubsetbuf(m_buffer.data(), m_buffer.capacity());
  }

  std::mutex m_mutex;
  std::filesystem::path m_filePath;
  std::vector<char> m_buffer;
  std::ofstream m_filestream;
  std::chrono::system_clock::time_point m_lastFlush{
      std::chrono::system_clock::now()};
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