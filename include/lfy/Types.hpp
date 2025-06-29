// Defines common types and constants used throughout the lfy project.

#pragma once

#include <array>
#include <string_view>

namespace lfy {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

constexpr std::string_view logLevelToString(LogLevel level) {
  constexpr static std::array logLevelMap = {"DEBUG", "INFO", "WARN", "ERROR"};
  return logLevelMap[static_cast<size_t>(level)];
}

} // namespace lfy