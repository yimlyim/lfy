// Defines common types and constants used throughout the lfy project.

#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <thread>


namespace lfy {

enum class LogLevel {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
  NumberOfLevels = 4
};

struct LogMetaData {
  LogMetaData(std::string name, LogLevel level)
      : m_loggerName{std::move(name)}, m_level{level} {}

  const std::string m_loggerName;
  const LogLevel m_level;
  const std::chrono::system_clock::time_point m_timestamp{
      std::chrono::system_clock::now()};
  const std::optional<std::thread::id> m_threadId{std::this_thread::get_id()};
};

constexpr std::string_view logLevelToString(LogLevel level) {
  constexpr static std::array logLevelMap = {"DEBUG", "INFO", "WARN", "ERROR"};
  static_assert(logLevelMap.size() ==
                    static_cast<size_t>(LogLevel::NumberOfLevels),
                "logLevelMap size must match number of LogLevel entries");
  return logLevelMap[static_cast<size_t>(level)];
}

} // namespace lfy