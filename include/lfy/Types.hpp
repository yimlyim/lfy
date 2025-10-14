// Defines common types and constants used throughout the lfy project.

#pragma once

#include <array>
#include <string_view>

namespace lfy {

enum class LogLevel {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
  NumberOfLevels = 4
};

constexpr std::string_view logLevelToString(LogLevel level) {
  constexpr static std::array logLevelMap = {"DEBUG", "INFO", "WARN", "ERROR"};
  static_assert(logLevelMap.size() ==
                    static_cast<size_t>(LogLevel::NumberOfLevels),
                "logLevelMap size must match number of LogLevel entries");
  return logLevelMap[static_cast<size_t>(level)];
}

} // namespace lfy