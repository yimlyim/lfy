#pragma once

#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>

namespace lfy {

template <typename T> class SegmentTrie {
public:
  struct TransparentHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const {
      return std::hash<std::string_view>{}(sv);
    }
  };

  struct TransparentEqual {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const {
      return lhs == rhs;
    }
  };

  struct Node {
    std::shared_ptr<T> m_value;
    std::unordered_map<std::string, Node, TransparentHash, TransparentEqual>
        m_children;
  };

  SegmentTrie() = default;
  SegmentTrie(std::shared_ptr<T> rootValue, std::string_view delimiter = ".")
      : m_root{std::move(rootValue)}, m_delimiter{delimiter} {};

  // Splits up a string by the "." character and inserts them into the trie. The
  // last segment points to the value.
  SegmentTrie<T> &insert(std::string_view key, std::shared_ptr<T> value) {
    std::reference_wrapper<Node> currentNode = m_root;
    for (auto segment : std::views::split(key, m_delimiter)) {
      std::string_view segmentView(segment.begin(), segment.end());
      auto child = currentNode.get().m_children.find(segmentView);
      if (child == currentNode.get().m_children.end()) {
        currentNode = currentNode.get()
                          .m_children.emplace(std::string{segmentView}, Node{})
                          .first->second;
        continue;
      }
      currentNode = child->second;
    }
    currentNode.get().m_value = std::move(value);
    return *this;
  }

  std::shared_ptr<T> find(std::string_view key) const {
    std::reference_wrapper<const Node> currentNode = m_root;
    for (const auto segment : std::views::split(key, m_delimiter)) {
      std::string_view segmentView(segment.begin(), segment.end());
      auto child = currentNode.get().m_children.find(segmentView);
      if (child == currentNode.get().m_children.end()) {
        // If one segment is not found, it means the whole key is not present
        return nullptr;
      }
      currentNode = child->second;
    }
    return currentNode.get().m_value;
  }

  std::shared_ptr<T> findByLongestPrefix(std::string_view key) const {
    std::reference_wrapper<const Node> currentNode = m_root;
    std::shared_ptr<T> lastValue = nullptr;

    for (const auto segment : std::views::split(key, m_delimiter)) {
      std::string_view segmentView(segment.begin(), segment.end());
      auto child = currentNode.get().m_children.find(segmentView);
      if (child == currentNode.get().m_children.end())
        break;

      if (child->second.m_value != nullptr)
        lastValue = child->second.m_value;

      currentNode = child->second;
    }

    return lastValue;
  }

  SegmentTrie<T> &remove(std::string_view key) {
    std::reference_wrapper<Node> currentNode = m_root;
    std::reference_wrapper<Node> parentNode = m_root;
    std::string_view lastSegment;

    for (const auto segment : std::views::split(key, m_delimiter)) {
      std::string_view segmentView(segment.begin(), segment.end());
      auto child = currentNode.get().m_children.find(segmentView);
      if (child == currentNode.get().m_children.end()) {
        // If one segment is not found, it means the whole key is not present,
        // thus nothing to remove
        return *this;
      }

      parentNode = currentNode;
      currentNode = child->second;
      lastSegment = segmentView;
    }

    // If we reach a leaf node, we can remove it from its parent, to avoid
    // memory build-up. Strictly speaking, we could also just set the value to
    // nullptr, and never really remove it
    currentNode.get().m_value = nullptr;
    if (currentNode.get().m_children.empty())
      parentNode.get().m_children.erase(std::string{lastSegment});
    return *this;
  }

private:
  Node m_root;
  const std::string m_delimiter = "."; // Delimiter used for splitting keys
};

} // namespace lfy