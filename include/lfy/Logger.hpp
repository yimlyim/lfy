// Responsible to format logging messages and manage log levels.
// Provides an interface to publish log messages to one outputter
#pragma once

#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Outputter.hpp"
#include "Types.hpp"

namespace lfy {

// For Thread Safety, never use a lambda which captures a state by reference.
using HeaderGenerator =
    std::function<void(const LogMetaData &, std::string &buffer)>;

using Flusher = std::function<void(const std::shared_ptr<Outputter>)>;

namespace flushers {

// Never manually flush the outputter. Let the outputter manage its own flushing
inline constexpr auto Automatic() {
  return [](const auto &) { return; };
}

// Always flush after each log message.
inline constexpr auto Always() {
  return [](const auto outputter) { outputter->flush(); };
}

// Flushes every N messages logged through the outputter.
inline constexpr auto EveryNthMessage(std::size_t n) {
  struct EveryN {
    std::size_t n;
    std::atomic<std::size_t> counter{0};
    void operator()(const std::shared_ptr<Outputter> &outputter) {
      // Each call increments; flush when reaching multiple of n.
      if (counter.fetch_add(1, std::memory_order_relaxed) % n == (n - 1))
        outputter->flush();
    }
  };

  return EveryN{n};
}

// Flushes, if the time since last flush exceeds the threshold. Does flush
// lazily, i.e. only when a log message is sent.
inline constexpr auto
LazyTimed(std::chrono::seconds threshold = std::chrono::seconds(1)) {
  return [threshold](const auto &outputter) {
    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    const std::chrono::steady_clock::time_point last = outputter->lastFlush();
    if (now - last >= threshold)
      outputter->flush();
  };
}

} // namespace flushers

class LogFormatter {
public:
  LogFormatter() = default;

  // Format the log message with headers and a format string and its arguments
  template <typename... Args>
  std::string format(const LogMetaData &metaData,
                     const std::vector<HeaderGenerator> &headers,
                     const std::format_string<Args...> &fmt,
                     Args &&...args) const {

    size_t estimated_size = 0;
    constexpr size_t avg_header_len = 32; // date, level, logger name
    constexpr size_t header_overhead = 4; // "[{}] "
    constexpr size_t avg_msg_len = 64;    // assumption: mostly short messages

    estimated_size += headers.size() * (avg_header_len + header_overhead);
    estimated_size += avg_msg_len;

    std::string result;
    result.reserve(estimated_size);
    for (const auto &headerGenerator : headers) {
      result.push_back('[');
      headerGenerator(metaData, result);
      result.append("] ");
    }
    std::format_to(std::back_inserter(result), fmt,
                   std::forward<Args>(args)...);
    return result;
  }

  template <>
  std::string format<>(const LogMetaData &metaData,
                       const std::vector<HeaderGenerator> &headers,
                       const std::format_string<> &msg) const {
    constexpr size_t avg_header_len = 32; // date, level, logger name
    constexpr size_t header_overhead = 4; // "[{}] "

    const size_t estimated_size =
        headers.size() * (avg_header_len + header_overhead) + msg.get().size();

    std::string result;
    result.reserve(estimated_size);
    for (const auto &headerGenerator : headers) {
      result.push_back('[');
      headerGenerator(metaData, result);
      result.append("] ");
    }
    result.append(msg.get());
    return result;
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
    LogMetaData metaData{m_name, LogLevel::Debug};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Info)
      return;
    LogMetaData metaData{m_name, LogLevel::Info};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Warn)
      return;
    LogMetaData metaData{m_name, LogLevel::Warn};
    log(m_formatter.format(metaData, m_headerGenerators, fmt,
                           std::forward<Args>(args)...));
  };

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args &&...args) {
    if (m_level > LogLevel::Error)
      return;
    LogMetaData metaData{m_name, LogLevel::Error};
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

  [[nodiscard]] const std::string &getName() const {
    // Thread-safe, as it's access to immutable member variable
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

  const std::string m_name;
  LogFormatter m_formatter{};
  std::atomic<LogLevel> m_level{LogLevel::Info};
  Flusher m_flushApplier{flushers::Automatic()};
};

} // namespace lfy