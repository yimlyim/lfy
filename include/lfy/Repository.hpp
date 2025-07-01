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
#include <unordered_map>

#include "Logger.hpp"
#include "SegmentTrie.hpp"

namespace lfy {

class Repository {
public:
  static std::shared_ptr<Logger> getLogger(const std::string &path,
                                           bool inheritFromParent = false) {
    Singleton &self = getInstance();
    std::lock_guard<std::mutex> lock(self.m_mutex);

    auto &loggers = self.m_loggers;
    if (auto it = loggers.find(path); it != loggers.end())
      return it->second;

    self.m_loggers[path] = std::shared_ptr<Logger>(new Logger());
    return self.m_loggers[path];
  }

  static void addLogger(const std::string &path,
                        std::shared_ptr<Logger> logger) {
    Singleton &self = getInstance();
    std::lock_guard<std::mutex> lock(self.m_mutex);

    self.m_loggers[path] = std::move(logger);
  }

private:
  Repository() = default;
  virtual ~Repository() = default;

  struct Singleton {
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;
    std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Logger>> m_loggers;
    SegmentTrie<Logger> m_radixTrie;
  };

  static Singleton &getInstance() {
    static Singleton instance;
    return instance;
  }
};

} // namespace lfy