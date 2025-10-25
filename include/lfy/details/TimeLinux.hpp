// Linux-specific time helpers
#pragma once

#include <ctime>
#include <time.h>

namespace lfy {

namespace details {

inline std::tm toUtcTm(std::time_t t) {
  std::tm tm;
  gmtime_r(&t, &tm);
  return tm;
}

inline std::tm toLocalTm(std::time_t t) {
  std::tm tm;
  localtime_r(&t, &tm);
  return tm;
}

inline int getLocalTimeZoneOffsetMinutes(const std::tm &localTm) {
  std::tm localCopy = localTm;
  std::tm utcCopy = localTm; // treat as UTC structure for conversion
  time_t localEpoch = mktime(&localCopy); // interprets as local time
  time_t utcEpoch = timegm(&utcCopy);     // interprets as UTC
  double diffSeconds = difftime(localEpoch, utcEpoch); // local - utc
  return static_cast<int>(diffSeconds / 60);
}

} // namespace details

} // namespace lfy