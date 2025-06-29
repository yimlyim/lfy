// Represents the output medium for log messages.
// This could be a file, console, or any other output, such as a network
// socket/memory buffers etc.

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <print>

namespace lfy {

class Outputter {
public:
  virtual ~Outputter() = default;
  virtual void output(const std::string &message) = 0;
  virtual void flush() = 0;
};

class ConsoleOutputter : public Outputter {
public:
  ConsoleOutputter() = default;
  ~ConsoleOutputter() = default;

  void output(const std::string &message) override {
    std::lock_guard l{m_mutex};
    std::println(std::cout, "{}", message);
  }

  void flush() override {
    std::lock_guard l{m_mutex};
    std::cout.flush();
  }

private:
  std::mutex m_mutex;
};

class FileOutputter : public Outputter {
public:
  explicit FileOutputter(const std::filesystem::path filePath)
      : m_filePath{std::move(filePath)} {
    if (std::filesystem::exists(m_filePath) &&
        !std::filesystem::is_regular_file(m_filePath))
      throw std::runtime_error("FileOutputter: File" + m_filePath.string() +
                               " does not exist or is not a regular file.");

    m_filestream.open(m_filePath, std::ios::out | std::ios::app);
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
  std::ofstream m_filestream;
};

} // namespace lfy