// Responsible to format logging messages and manage log levels.
// Provides an interface to publish log messages to one outputter
#pragma once

#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Outputter.hpp"
#include "Types.hpp"

namespace lfy {

// Flushing Policies define how the outputter should handle flushing of log
// messages. It is globally set for all outputters in a logger.
enum class FlushingPolicy { AlwaysFlush, PeriodicFlush, NeverFlush };

struct LogMetaData;
// For Thread Safety, never use a lambda which captures a state by reference.
using HeaderGenerator = std::function<std::string(const LogMetaData &)>;

struct LogMetaData {
  explicit LogMetaData(LogLevel level) : m_level{level} {}

  const LogLevel m_level;
  const std::chrono::system_clock::time_point m_timestamp{
      std::chrono::system_clock::now()};
  const std::thread::id m_threadId{std::this_thread::get_id()};
};

namespace headergen {
inline constexpr auto Level = [](const LogMetaData &metaData) {
  return std::string{logLevelToString(metaData.m_level)};
};
inline constexpr auto Time = [](const LogMetaData &metaData) {
  const std::chrono::time_point now =
      std::chrono::floor<std::chrono::seconds>(metaData.m_timestamp);
  return std::format("{:%FT%R:%S%Ez}", now);
};
} // namespace headergen

class LogFormatter {
public:
  LogFormatter() = default;

  template <typename... Args>
  std::string format(const LogMetaData &metaData,
                     const std::vector<HeaderGenerator> &headers,
                     const std::format_string<Args...> &fmt,
                     Args &&...args) const {
    std::stringstream ss;
    for (const auto &header : headers)
      ss << std::format("[{}] ", header(metaData));

    return std::format("{}{}", ss.str(),
                       std::format(fmt, std::forward<Args>(args)...));
  }
};

class Logger {
public:
  Logger() = default;
  Logger(std::vector<std::shared_ptr<Outputter>> outputters,
         std::vector<HeaderGenerator> headerGenerators, LogLevel logLevel)
      : m_outputters(std::move(outputters)),
        m_headerGenerators{std::move(headerGenerators)}, m_level{logLevel} {}
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void log(const std::string &message) {
    for (const auto &outputter : m_outputters)
      outputter->output(message);

    if (m_flushingPolicy == FlushingPolicy::AlwaysFlush)
      for (const auto &outputter : m_outputters)
        outputter->flush();
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level < LogLevel::Debug)
      return;

    LogMetaData metaData{LogLevel::Debug};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level < LogLevel::Info)
      return;

    LogMetaData metaData{LogLevel::Info};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level < LogLevel::Warn)
      return;

    LogMetaData metaData{LogLevel::Warn};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level < LogLevel::Error)
      return;

    LogMetaData metaData{LogLevel::Error};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  Logger &addOutputter(std::shared_ptr<Outputter> outputter) {
    std::lock_guard lock(m_mutex);
    m_outputters.push_back(std::shared_ptr<Outputter>(std::move(outputter)));
    return *this;
  }

  Logger &addHeaderGenerator(HeaderGenerator generator) {
    std::lock_guard l{m_mutex};
    m_headerGenerators.push_back(std::move(generator));
    return *this;
  }

  Logger &setLogLevel(LogLevel level) {
    // Atomic assignment is thread-safe, no need for a lock here.
    m_level = level;
    return *this;
  }

  Logger &setFlushingPolicy(FlushingPolicy policy) {
    // Atomic assignment is thread-safe, no need for a lock here.
    m_flushingPolicy = policy;
    return *this;
  }

  [[nodiscard]] std::string getName() const { return m_name; }

  [[nodiscard]] std::vector<std::shared_ptr<Outputter>> getOutputters() const {
    std::lock_guard l{m_mutex};
    return m_outputters;
  }

  [[nodiscard]] std::vector<HeaderGenerator> getHeaderGenerators() const {
    std::lock_guard l{m_mutex};
    return m_headerGenerators;
  }

  [[nodiscard]] LogLevel getLogLevel() const { return m_level; }

  [[nodiscard]] FlushingPolicy getFlushingPolicy() const {
    return m_flushingPolicy;
  }

private:
  mutable std::mutex m_mutex;
  std::vector<std::shared_ptr<Outputter>> m_outputters;
  std::vector<HeaderGenerator> m_headerGenerators;

  std::string m_name;
  LogFormatter m_formatter;
  std::atomic<LogLevel> m_level{LogLevel::Info};
  std::atomic<FlushingPolicy> m_flushingPolicy{FlushingPolicy::AlwaysFlush};
};

} // namespace lfy