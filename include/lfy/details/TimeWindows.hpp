// Windows-specific time helpers
#pragma once

#ifndef _WIN32
#error "TimeWindows.hpp included on non-Windows platform"
#endif

#include <timezoneapi.h>
#include <windows.h>

#include <ctime>

namespace details {

inline std::tm toUtcTm(std::time_t t) {
  std::tm tm;
  gmtime_s(&tm, &t);
  return tm;
}

inline std::tm toLocalTm(std::time_t t) {
  std::tm tm;
  localtime_s(&tm, &t);
  return tm;
}

// Resolve current LOCAL timezone offset in minutes accounting for DST.
inline int getLocalTimeZoneOffsetMinutes(const std::tm &time) {
  DYNAMIC_TIME_ZONE_INFORMATION dtzi;
  ::GetDynamicTimeZoneInformation(&dtzi);
  // Windows bias minutes are negative of actual offset; invert and include DST.
  int offset = -(dtzi.Bias +
                 ((time.tm_isdst > 0) ? dtzi.DaylightBias : dtzi.StandardBias));
  return offset;
}

} // namespace details
