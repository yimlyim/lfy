// Defines common types and constants used throughout the Log4Yum project.

#pragma once

#include <array>
#include <string_view>

namespace log4yum {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

constexpr std::string_view logLevelToString(LogLevel level) {
  constexpr static std::array logLevelMap = {"DEBUG", "INFO", "WARN", "ERROR"};
  return logLevelMap[static_cast<size_t>(level)];
}

} // namespace log4yum