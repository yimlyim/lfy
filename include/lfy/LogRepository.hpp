// Defines a LogRepository class that manages loggers and their configurations.
// a logger is identified by a path, which is a string. Dot separated paths are
// used to identify nested loggers. The repository uses each path item to allow
// for hierarchical loggers, which inherit properties from their parent loggers,
// such as outputters and log levels. The levels are overwriteable, meaning that
// a child logger can have a different log level than its parent.
#pragma once

#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Logger.hpp"

namespace lfy {

class LogRepository {
public:
  LogRepository() = default;
  virtual ~LogRepository() = default;

  std::shared_ptr<Logger> getLogger(const std::string &path) const {
    std::lock_guard<std::mutex> lock(getSingleton().m_mutex);
    auto &loggers = getSingleton().m_loggers;

    if (auto it = loggers.find(path); it != loggers.end()) {
      return it->second;
    } else {
      loggers[path] = std::make_shared<Logger>();
      return loggers[path];
    }
  }

  std::expected<void, std::string> addLogger(const std::string &path,
                                             std::shared_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lock(getSingleton().m_mutex);
    auto &loggers = getSingleton().m_loggers;

    if (auto it = loggers.find(path); it == loggers.end()) {
      loggers[path] = logger;
      return std::expected<void, std::string>{};
    } else {
      return std::unexpected{"Logger with path '" + path + "' already exists."};
    }
  }

private:
  struct Singleton {
    std::unordered_map<std::string, std::shared_ptr<Logger>> m_loggers;
    std::mutex m_mutex;
  };

  Singleton &getSingleton() const {
    static Singleton instance;
    return instance;
  }
};

} // namespace lfy