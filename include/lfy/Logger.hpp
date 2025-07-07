// Responsible to format logging messages and manage log levels.
// Provides an interface to publish log messages to one outputter
#pragma once

#include <chrono>
#include <format>
#include <functional>
#include <iomanip>
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
enum class TimeType { Local, Utc };

namespace details {

inline std::string resolveTimeZoneOffset(const std::string &fmt, std::time_t t,
                                         TimeType timeType) {
  std::string result = fmt;
  std::string tz_offset;
  if (result.find("%z") != std::string::npos) {
    if (timeType == TimeType::Utc) {
      tz_offset = "+0000";
    } else {
      std::tm tm_local, tm_utc;
#if defined(_WIN32)
      localtime_s(&tm_local, &t);
      gmtime_s(&tm_utc, &t);
#else
      localtime_r(&t, &tm_local);
      gmtime_r(&t, &tm_utc);
#endif
      int offset =
          static_cast<int>(std::mktime(&tm_local) - std::mktime(&tm_utc));
      char sign = (offset >= 0) ? '+' : '-';
      offset = std::abs(offset);
      int hours = offset / 3600;
      int minutes = (offset % 3600) / 60;
      std::ostringstream oss;
      oss << sign << std::setw(2) << std::setfill('0') << hours << std::setw(2)
          << std::setfill('0') << minutes;
      tz_offset = oss.str();
    }
    // Replace all occurrences of %z
    size_t pos = 0;
    while ((pos = result.find("%z", pos)) != std::string::npos) {
      result.replace(pos, 2, tz_offset);
      pos += tz_offset.length();
    }
  }
  return result;
}

} // namespace details

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

inline auto Level() {
  return [](const LogMetaData &metaData) {
    return std::string{logLevelToString(metaData.m_level)};
  };
}

inline auto _InternalTime(const std::string &fmt, const LogMetaData &metaData,
                          TimeType timeType) {
  using CachedTime =
      std::pair<std::string, std::chrono::system_clock::time_point>;
  thread_local static std::unordered_map<std::string, CachedTime> cachedTimes;

  const auto now =
      std::chrono::floor<std::chrono::seconds>(metaData.m_timestamp);
  const auto last = (cachedTimes.find(fmt) != cachedTimes.end())
                        ? cachedTimes[fmt].second
                        : std::chrono::system_clock::time_point{};
  if (now - last >= std::chrono::seconds(1)) {
    std::ostringstream oss;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#if defined(_WIN32)
    if (timeType == TimeType::Utc)
      gmtime_s(&tm, &t); // Windows
    else
      localtime_s(&tm, &t); // Windows
#else
    if (timeType == TimeType::Utc)
      gmtime_r(&t, &tm); // POSIX
    else
      localtime_r(&t, &tm); // POSIX
#endif
    std::string replacedFmt = details::resolveTimeZoneOffset(fmt, t, timeType);
    oss << std::put_time(&tm, replacedFmt.data());
    cachedTimes[fmt] = {oss.str(), now};
  }
  return cachedTimes[fmt].first;
}

inline auto Time(TimeType timeType = TimeType::Local,
                 std::string fmt = "%Y-%m-%dT%H:%M:%S%z") {
  return [fmt, timeType](const LogMetaData &metaData) {
    return _InternalTime(fmt, metaData, timeType);
  };
}

inline auto LoggerName() {
  return [](const LogMetaData &metaData) { return metaData.m_loggerName; };
};

} // namespace headergen

using Flusher = std::function<void(const std::shared_ptr<Outputter>)>;

namespace flushers {
inline constexpr auto NeverFlush() {
  return [](const auto &) { return; };
};

inline constexpr auto AlwaysFlush() {
  return [](const auto outputter) { outputter->flush(); };
}

inline constexpr auto
TimeThresholdFlush(std::chrono::seconds threshold = std::chrono::seconds(1)) {
  return [threshold](const auto &outputter) {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    const system_clock::time_point now = system_clock::now();
    const system_clock::time_point last = outputter->lastFlush();
    if (now - last >= threshold)
      outputter->flush();
  };
}

} // namespace flushers

class LogFormatter {
public:
  LogFormatter() = default;

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
    for (const auto &header : headers)
      std::format_to(std::back_inserter(result), "[{}] ", header(metaData));
    std::format_to(std::back_inserter(result), "{}",
                   std::format(fmt, std::forward<Args>(args)...));

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
    // Thread-safe, as it's access to const member variable
    return m_name;
  }

  [[nodiscard]] std::string_view getNameView() const {
    // Thread-safe, as it's acess to const member variable
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
  Flusher m_flushApplier{flushers::AlwaysFlush()};
};

} // namespace lfy