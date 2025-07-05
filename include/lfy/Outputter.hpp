// Represents the output medium for log messages.
// This could be a file, console, or any other output, such as a network
// socket/memory buffers etc.

#pragma once

#include <cstddef>
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
    // Don't use std::print, as it *always* causes flush for terminal output
    std::cout << message << "\n";
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    std::cout.flush();
  }

private:
  std::mutex m_mutex;
  std::vector<char> m_buffer;
};

class FileOutputter : public Outputter {
public:
  explicit FileOutputter(const std::filesystem::path filePath,
                         std::size_t bufferSize = 16 * literals::KiB)
      : m_filePath{std::move(filePath)} {
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

  ~FileOutputter() = default;
  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};

    std::println(m_filestream, "{}", message);
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    if (m_filestream.is_open())
      m_filestream.flush();
  }

private:
  std::mutex m_mutex;
  std::filesystem::path m_filePath;
  std::vector<char> m_buffer;
  std::ofstream m_filestream;
};

} // namespace lfy