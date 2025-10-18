#pragma once

#include "Logger.hpp"
#include <string_view>
#include <unordered_map>

namespace lfy {

class SegmentMap {
public:
  SegmentMap() = default;

  // Inserts a logger into the map. Replaces any existing logger with the same
  // name.
  SegmentMap &insert(std::shared_ptr<Logger> logger) {
    std::string_view key = logger->getName();
    m_map.insert_or_assign(std::move(key), std::move(logger));
    return *this;
  }

  // Exact match find
  std::shared_ptr<Logger> find(std::string_view key) const {
    auto it = m_map.find(key);
    return (it != m_map.end()) ? it->second : nullptr;
  }

  // Finds the logger by the longest matching segment prefix.
  // E.g., if the map contains loggers for "app", "app.module",
  // and the key is "app.module.submodule", it will return the logger for
  // "app.module".
  std::shared_ptr<Logger> findByLongestPrefix(std::string_view key) const {
    // Direct match first
    if (auto val = find(key); val)
      return val;

    // If no direct match, search for the longest prefix
    for (auto i = key.size() - 1; i > 0; --i) {
      if (key[i] != m_delimiter)
        continue;

      std::string_view substr = key.substr(0, i);
      auto it = m_map.find(substr);
      if (it != m_map.end())
        return it->second;
    }

    // If no prefix match, return the default logger if it exists
    if (auto defaultLoggerIt = m_map.find(""); defaultLoggerIt != end(m_map))
      return defaultLoggerIt->second;

    return nullptr;
  }

  void remove(std::string_view key) { m_map.erase(key); }

private:
  // Idea: Each logger contains a name, so we can use that as the key. To be
  // extra memory efficient. A logger name can be segmented by a delimiter
  // (e.g., '.'), which can be used to identify parent-child relationships.
  // For each two logger names a,b , where a is a full prefix (from start to
  // delimiter) of b, we can consider a as the parent of b.
  // e.g. "app.module" is parent of "app.module.submodule".
  // All keys share the default logger as their ultimate parent.
  std::unordered_map<std::string_view, std::shared_ptr<Logger>> m_map;
  const char m_delimiter = '.';
};

} // namespace lfy