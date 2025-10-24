#pragma once

#include <chrono>
#include <iomanip>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <timezoneapi.h>
#include <windows.h>
#endif

#ifdef __linux__
#include <time.h>
#include <unistd.h>
#endif

#include "lfy/Types.hpp"

namespace lfy {

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
    return "+00:00";

  char sign = (offsetInMinutes >= 0) ? '+' : '-';
  offsetInMinutes = std::abs(offsetInMinutes);
  int hours = offsetInMinutes / 60;
  int minutes = offsetInMinutes % 60;
  return std::format("{}{:02}:{:02}", sign, hours, minutes);
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

namespace headergen {

inline auto Level() {
  return [](const LogMetaData &metaData, std::string &buffer) {
    buffer.append(logLevelToString(metaData.m_level));
  };
}

inline auto _InternalTime(const std::string &fmt, const LogMetaData &metaData,
                          TimeType timeType, std::string &buffer) {
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

  if (now - last < literals::timeStampRefreshInterval) {
    buffer.append(cachedTime.lastFormattedTime);
    return;
  }

  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  const std::tm tm = details::toPlatformSpecificTm(t, timeType);

  if (timeType == TimeType::Local &&
      (now - last >= literals::timeZoneOffsetRefreshInterval))
    cachedLocalTimeZoneOffset = details::getTimeZoneOffsetInMinutes(tm);

  std::string formattedTime = details::toFormattedTime(
      fmt, tm, (timeType == TimeType::Local) ? cachedLocalTimeZoneOffset : 0);
  cachedTime = details::TimeCache{std::move(formattedTime), now};
  buffer.append(cachedTime.lastFormattedTime);
}

inline auto Time(TimeType timeType = TimeType::Local,
                 std::string fmt = "%Y-%m-%dT%H:%M:%S%z") {
  return [fmt, timeType](const LogMetaData &metaData, std::string &buffer) {
    return _InternalTime(fmt, metaData, timeType, buffer);
  };
}

inline auto LoggerName() {
  return [](const LogMetaData &metaData, std::string &buffer) {
    buffer.append(metaData.m_loggerName);
  };
};

} // namespace headergen

} // namespace lfy