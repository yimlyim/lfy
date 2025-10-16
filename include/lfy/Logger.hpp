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
#include <unordered_map>
#include <vector>

#include "Outputter.hpp"
#include "Types.hpp"

#ifdef _WIN32
#include <timezoneapi.h>
#include <windows.h>
#endif

namespace lfy {

struct LogMetaData;
enum class TimeType { Local, Utc };

namespace literals {

constexpr auto timeZoneOffsetRefreshInterval = std::chrono::seconds(10);
constexpr auto timeStampRefreshInterval = std::chrono::seconds(1);

} // namespace literals

namespace details {

struct TimeCache {
  std::string lastFormattedTime; // Cached formatted time string (e.g.
                                 // "2024-10-05T14:23:45+0200")
  std::chrono::system_clock::time_point
      lastTimePoint; // Last time point when the time was formatted
};

inline std::tm toPlatformSpecificTm(std::time_t t, TimeType timeType) {
#if defined(_WIN32)
  std::tm tm;
  if (timeType == TimeType::Utc)
    gmtime_s(&tm, &t);
  else
    localtime_s(&tm, &t);
  return tm;
#elif defined(__linux__)
  std::tm tm;
  if (timeType == TimeType::Utc)
    gmtime_r(&t, &tm);
  else
    localtime_r(&t, &tm);
#else
  throw std::runtime_error("makePlatformSpecificTm: Unsupported platform");
#endif
}

inline std::string toNumericUtcOffset(int offsetInMinutes) {
  if (offsetInMinutes == 0)
    return "+0000";

  char sign = (offsetInMinutes >= 0) ? '+' : '-';
  offsetInMinutes = std::abs(offsetInMinutes);
  int hours = offsetInMinutes / 60;
  int minutes = offsetInMinutes % 60;
  return std::format("{}{:02}{:02}", sign, hours, minutes);
}

inline int getTimeZoneOffsetInMinutes(const std::tm &time) {
  int offsetInMinutes;
#if defined(_WIN32)
  DYNAMIC_TIME_ZONE_INFORMATION dtzi;
  ::GetDynamicTimeZoneInformation(&dtzi);
  offsetInMinutes = -(dtzi.Bias + ((time.tm_isdst > 0) ? dtzi.DaylightBias
                                                       : dtzi.StandardBias));
#elif defined(__linux__)
  offsetInMinutes = time.tm_gmtoff / 60;
#else
  throw std::runtime_error("resolveTimeZoneOffset: Unsupported platform");
#endif
  return offsetInMinutes;
}

inline std::string toFormattedTime(const std::string &fmt, const std::tm &tm,
                                   int timeZoneOffset) {
  // Handle %z manually, as std::put_time does not consistently correctly
  // support it
  auto fmtCopy = fmt;
  std::string::size_type pos = 0;
  while ((pos = fmtCopy.find("%z", pos)) != std::string::npos) {
    const std::string offsetStr = toNumericUtcOffset(timeZoneOffset);
    fmtCopy.replace(pos, 2, offsetStr);
    pos += offsetStr.size();
  }

  std::ostringstream oss;
  oss << std::put_time(&tm, fmtCopy.data());
  return oss.str();
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
  // Caches a specific time format string to the last formatted time e.g. fmt:
  // "%Y-%m-%dT%H:%M:%S%z" -> ("2024-10-05T14:23:45+0200",
  // TimePoint(2024-10-05T14:23:45))
  thread_local static std::unordered_map<
      TimeType, std::unordered_map<std::string, details::TimeCache>>
      cachedTimes;
  thread_local static int cachedLocalTimeZoneOffset{0};
  // The idea is the following: The first time this function is called with a
  // specific format string, we default construct the "last" time point to
  // epoch (1970-01-01). This guarantees that the difference between "now" and
  // "last" is always greater than one second, for the first call.
  details::TimeCache &cachedTime = cachedTimes[timeType][fmt];
  const auto now =
      std::chrono::floor<std::chrono::seconds>(metaData.m_timestamp);
  const auto last = cachedTime.lastTimePoint;

  if (now - last < literals::timeStampRefreshInterval)
    return cachedTime.lastFormattedTime;

  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  const std::tm tm = details::toPlatformSpecificTm(t, timeType);

  if (timeType == TimeType::Local &&
      (now - last >= literals::timeZoneOffsetRefreshInterval))
    cachedLocalTimeZoneOffset = details::getTimeZoneOffsetInMinutes(tm);

  std::string formattedTime = details::toFormattedTime(
      fmt, tm, (timeType == TimeType::Local) ? cachedLocalTimeZoneOffset : 0);
  cachedTime = details::TimeCache{std::move(formattedTime), now};
  return cachedTime.lastFormattedTime;
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
    for (const auto &headerGenerator : headers) {
      const std::string header = headerGenerator(metaData);
      result.push_back('[');
      result.append(header);
      result.append("] ");
    }

    std::format_to(std::back_inserter(result), fmt,
                   std::forward<Args>(args)...);
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