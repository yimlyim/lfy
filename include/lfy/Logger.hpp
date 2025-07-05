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

struct LogMetaData;
// For Thread Safety, never use a lambda which captures a state by reference.
using HeaderGenerator = std::function<std::string(const LogMetaData &)>;

struct LogMetaData {
  LogMetaData(std::string name, LogLevel level)
      : m_loggerName{std::move(name)}, m_level{level} {}

  const std::string m_loggerName;
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

inline constexpr auto LoggerName = [](const LogMetaData &metaData) {
  // This is a placeholder, as the logger name is not part of LogMetaData.
  // In practice, you would pass the logger name as an additional parameter.
  return metaData.m_loggerName;
};
} // namespace headergen

using Flusher = std::function<void(const std::shared_ptr<Outputter>)>;

namespace flushers {
inline constexpr auto None = [](const auto _) {
  // Flushing is disabled, do nothing. The compiler will optimize this out.
  return;
};
inline constexpr auto AlwaysFlush = [](const auto outputter) {
  outputter->flush();
};
inline constexpr auto TimeThresholdFlush = [](const auto &outputter) {
  using OutputterPtr = const void *;
  static std::mutex m;
  static std::unordered_map<OutputterPtr, std::chrono::system_clock::time_point>
      lastFlushMap;

  std::lock_guard lock(m);
  auto now = std::chrono::system_clock::now();
  auto &lastFlush = lastFlushMap[static_cast<OutputterPtr>(outputter.get())];
  if (now - lastFlush >= std::chrono::seconds(1)) {
    outputter->flush();
    lastFlush = now;
  }
};
} // namespace flushers

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

class Repository;
class Logger {
  friend class lfy::Repository;

public:
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void log(const std::string &message) {
    for (const auto &outputter : m_outputters) {
      outputter->output(message);
      m_flushApplier(outputter);
    }
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Debug)
      return;

    LogMetaData metaData{getName(), LogLevel::Debug};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Info)
      return;

    LogMetaData metaData{getName(), LogLevel::Info};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Warn)
      return;

    LogMetaData metaData{getName(), LogLevel::Warn};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Error)
      return;

    LogMetaData metaData{getName(), LogLevel::Error};
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
    m_level = level;
    return *this;
  }

  Logger &setFlusher(Flusher flushApplier) {
    std::lock_guard l{m_mutex};
    m_flushApplier = std::move(flushApplier);
    return *this;
  }

  [[nodiscard]] std::string getName() const {
    std::lock_guard l{m_mutex};
    return m_name;
  }

  [[nodiscard]] std::string_view getNameView() const {
    std::lock_guard l{m_mutex};
    return m_name;
  }

  [[nodiscard]] LogFormatter getFormatter() const {
    std::lock_guard l{m_mutex};
    return m_formatter;
  }

  [[nodiscard]] std::vector<std::shared_ptr<Outputter>> getOutputters() const {
    std::lock_guard l{m_mutex};
    return m_outputters;
  }

  [[nodiscard]] std::vector<HeaderGenerator> getHeaderGenerators() const {
    std::lock_guard l{m_mutex};
    return m_headerGenerators;
  }

  [[nodiscard]] LogLevel getLogLevel() const { return m_level; }

  [[nodiscard]] Flusher getFlusher() const {
    std::lock_guard l{m_mutex};
    return m_flushApplier;
  }

private:
  Logger() = default;
  Logger(std::string name) : m_name{std::move(name)} {}
  Logger(std::vector<std::shared_ptr<Outputter>> outputters,
         std::vector<HeaderGenerator> headerGenerators, std::string name,
         LogFormatter formatter, LogLevel logLevel, Flusher flusher)
      : m_outputters(std::move(outputters)),
        m_headerGenerators(std::move(headerGenerators)),
        m_name(std::move(name)), m_formatter(std::move(formatter)),
        m_level(logLevel), m_flushApplier(std::move(flusher)) {}

  mutable std::mutex m_mutex;
  std::vector<std::shared_ptr<Outputter>> m_outputters;
  std::vector<HeaderGenerator> m_headerGenerators;

  std::string m_name;
  LogFormatter m_formatter{};
  std::atomic<LogLevel> m_level{LogLevel::Info};
  Flusher m_flushApplier{flushers::AlwaysFlush};
};

} // namespace lfy