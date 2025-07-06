#pragma once

#include "Logger.hpp"
#include <string_view>
#include <unordered_map>

namespace lfy {

class SegmentMap {
public:
  SegmentMap() = default;
  // Splits up a string by the "." character and inserts them into the trie. The
  // last segment points to the value.
  SegmentMap &insert(std::string_view key, std::shared_ptr<Logger> value) {
    if (key != value->getNameView())
      return *this;
    m_map[value->getNameView()] = std::move(value);
    return *this;
  }

  std::shared_ptr<Logger> find(std::string_view key) const {
    auto it = m_map.find(key);
    return (it != m_map.end()) ? it->second : nullptr;
  }

  std::shared_ptr<Logger> findByLongestPrefix(std::string_view key) const {
    // Direct match first
    if (auto val = find(key); val)
      return val;

    // If no direct match, search for the longest prefix
    for (int i = key.size() - 1; i > 0; --i) {
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
  // extra memory efficient.
  std::unordered_map<std::string_view, std::shared_ptr<Logger>> m_map;
  const char m_delimiter = '.';
};

} // namespace lfy