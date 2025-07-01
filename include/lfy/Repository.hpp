// Defines a LogRepository class that manages loggers and their configurations.
// a logger is identified by a path, which is a string. Dot separated paths are
// used to identify nested loggers. The repository uses each path item to allow
// for hierarchical loggers, which inherit properties from their parent loggers,
// such as outputters and log levels. The levels are overwriteable, meaning that
// a child logger can have a different log level than its parent.
#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "Logger.hpp"
#include "SegmentTrie.hpp"

namespace lfy {

class Repository {
public:
  // Retrieves a logger by its path. For inheritance, if no exact match is
  // found, it will search for the longest prefix match and create a new logger
  // that inherits all settings from the parent logger, if any.
  static std::shared_ptr<Logger> getLogger(const std::string &path,
                                           bool inheritFromParent = false) {
    Singleton &self = getInstance();
    std::lock_guard<std::mutex> lock(self.m_mutex);

    // Try exact match first
    if (auto logger = self.m_loggers.find(path); logger != nullptr) {
      return logger;
    }

    // If there was no exact match and inheritance is requested,
    // create a new logger by inheriting from the longest prefix match (if any)
    if (inheritFromParent) {
      if (auto parentLogger = self.m_loggers.findByLongestPrefix(path);
          parentLogger != nullptr) {
        auto inheritedLogger = std::shared_ptr<Logger>(new Logger(
            parentLogger->getOutputters(), parentLogger->getHeaderGenerators(),
            path, parentLogger->getFormatter(), parentLogger->getLogLevel(),
            parentLogger->getFlusher()));
        self.m_loggers.insert(path, inheritedLogger);
        return inheritedLogger;
      }
    }

    // Create a new logger if none found or inherited
    auto newLogger = std::shared_ptr<Logger>(new Logger(path));
    self.m_loggers.insert(path, newLogger);
    return newLogger;
  }

  // Adds a logger to the repository with the specified path.
  // If a logger with the same path already exists, it will be overwritten.
  static void addLogger(const std::string &path,
                        std::shared_ptr<Logger> logger) {
    Singleton &self = getInstance();
    std::lock_guard<std::mutex> lock(self.m_mutex);

    self.m_loggers.insert(path, logger);
  }

private:
  Repository() = default;
  virtual ~Repository() = default;

  struct Singleton {
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
    std::mutex m_mutex;
    SegmentTrie<Logger> m_loggers;
  };

  static Singleton &getInstance() {
    static Singleton instance;
    return instance;
  }
};

} // namespace lfy