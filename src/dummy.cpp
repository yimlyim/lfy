#include <chrono>
#include <cstddef>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "lfy/Logger.hpp"
#include "lfy/Outputter.hpp"
#include "lfy/Repository.hpp"
#include "lfy/SegmentMap.hpp"
#include "lfy/SegmentTrie.hpp"
/*
void benchmarkTrieVsMap(size_t numKeys = 100000, size_t numLookups = 100000) {
  using namespace std::chrono;
  using lfy::SegmentMap;
  using lfy::SegmentTrie;

  // Generate dot-separated keys with varying segment lengths
  std::vector<std::string> keys;
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> numSegDist(2, 30); // 2-6 segments
  std::uniform_int_distribution<int> segLenDist(2,
                                                20); // 2-8 chars per segment
  std::uniform_int_distribution<int> charDist('a', 'z');
  for (size_t i = 0; i < numKeys; ++i) {
    std::string key;
    int segments = numSegDist(rng);
    for (int s = 0; s < segments; ++s) {
      if (s > 0)
        key += '.';
      int len = segLenDist(rng);
      for (int l = 0; l < len; ++l)
        key += static_cast<char>(charDist(rng));
    }
    keys.push_back(std::move(key));
  }

  // Prepare values
  std::vector<std::shared_ptr<std::string>> values;
  for (size_t i = 0; i < numKeys; ++i)
    values.push_back(
        std::make_shared<std::string>("value_" + std::to_string(i)));

  // Benchmark SegmentTrie insertion
  SegmentTrie<std::string> trie;
  auto start = high_resolution_clock::now();
  for (size_t i = 0; i < numKeys; ++i)
    trie.insert(keys[i], values[i]);
  auto end = high_resolution_clock::now();
  auto trieInsertMs = duration_cast<milliseconds>(end - start).count();
  std::cout << "SegmentTrie insert: "
            << trieInsertMs << " ms\n";

  // Benchmark unordered_map insertion
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
  std::unordered_map<std::string, std::shared_ptr<std::string>, TransparentHash,
                     TransparentEqual>
      map;
  start = high_resolution_clock::now();
  for (size_t i = 0; i < numKeys; ++i)
    map[keys[i]] = values[i];
  end = high_resolution_clock::now();
  auto mapInsertMs = duration_cast<milliseconds>(end - start).count();
  std::cout << "unordered_map insert: "
            << mapInsertMs << " ms\n";

  // Benchmark SegmentMap insertion
  SegmentMap<std::string> segmap;
  start = high_resolution_clock::now();
  for (size_t i = 0; i < numKeys; ++i)
    segmap.insert(keys[i], values[i]);
  end = high_resolution_clock::now();
  auto segMapInsertMs = duration_cast<milliseconds>(end - start).count();
  std::cout << "SegmentMap insert: "
            << segMapInsertMs << " ms\n";

  // Prepare random lookup indices
  std::uniform_int_distribution<size_t> idxDist(0, numKeys - 1);
  std::vector<std::string_view> lookupKeys;
  for (size_t i = 0; i < numLookups; ++i)
    lookupKeys.push_back(keys[idxDist(rng)]);

  // Benchmark SegmentTrie search
  size_t foundTrie = 0;
  start = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (trie.find(key))
      ++foundTrie;
  end = high_resolution_clock::now();
  auto trieSearchMs = duration_cast<milliseconds>(end - start).count();

  // Benchmark SegmentTrie prefix search
  size_t foundTriePrefix = 0;
  start = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (trie.findByLongestPrefix(key))
      ++foundTriePrefix;
  end = high_resolution_clock::now();
  auto triePrefixSearchMs = duration_cast<milliseconds>(end - start).count();

  // Benchmark unordered_map search
  size_t foundMap = 0;
  start = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (map.find(key) != map.end())
      ++foundMap;
  end = high_resolution_clock::now();
  auto mapSearchMs = duration_cast<milliseconds>(end - start).count();

  // Benchmark SegmentMap search
  size_t foundSegMap = 0;
  start = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (segmap.find(key))
      ++foundSegMap;
  end = high_resolution_clock::now();
  auto segMapSearchMs = duration_cast<milliseconds>(end - start).count();

  // Benchmark SegmentMap prefix search
  size_t foundSegMapPrefix = 0;
  start = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (segmap.findByLongestPrefix(key))
      ++foundSegMapPrefix;
  end = high_resolution_clock::now();
  auto segMapPrefixSearchMs = duration_cast<milliseconds>(end - start).count();

  // Print all results in requested format
  std::cout << "\nBenchmark results:\n";
  std::cout << "SegmentTrie:    insert " << trieInsertMs
            << " ms // search " << trieSearchMs << " ms // prefix-search " <<
triePrefixSearchMs << " ms\n"; std::cout << "  found:        " << foundTrie <<
"/" << numLookups << " // prefix-found: " << foundTriePrefix << "/" <<
numLookups << "\n"; std::cout << "unordered_map:  insert " << mapInsertMs
            << " ms // search " << mapSearchMs << " ms\n";
  std::cout << "  found:        " << foundMap << "/" << numLookups << "\n";
  std::cout << "SegmentMap:     insert " << segMapInsertMs
            << " ms // search " << segMapSearchMs << " ms // prefix-search " <<
segMapPrefixSearchMs << " ms\n"; std::cout << "  found:        " << foundSegMap
<< "/" << numLookups << " // prefix-found: " << foundSegMapPrefix << "/" <<
numLookups << "\n";
}

void benchmarkTrieVsMapPrefixHeavy(size_t depth = 8, size_t branches = 8,
                                   size_t lookups = 100000) {
  using namespace std::chrono;
  using lfy::SegmentMap;
  using lfy::SegmentTrie;

  // Generate a prefix tree of keys: e.g. com, com.example, com.example.foo, ...
  std::vector<std::string> keys;
  std::function<void(std::string, size_t)> gen = [&](std::string prefix,
                                                     size_t d) {
    if (d == depth)
      return;
    for (size_t b = 0; b < branches; ++b) {
      std::string seg = "seg" + std::to_string(b);
      std::string key = prefix.empty() ? seg : prefix + "." + seg;
      keys.push_back(key);
      gen(key, d + 1);
    }
  };
  gen("", 0);

  // Prepare values
  std::vector<std::shared_ptr<std::string>> values;
  for (size_t i = 0; i < keys.size(); ++i)
    values.push_back(
        std::make_shared<std::string>("value_" + std::to_string(i)));

  // Insert into SegmentTrie
  SegmentTrie<std::string> trie;
  auto start = high_resolution_clock::now();
  for (size_t i = 0; i < keys.size(); ++i)
    trie.insert(keys[i], values[i]);
  auto end = high_resolution_clock::now();
  auto trieInsertMs = duration_cast<milliseconds>(end - start).count();

  // Insert into SegmentMap
  SegmentMap<std::string> segmap;
  start = high_resolution_clock::now();
  for (size_t i = 0; i < keys.size(); ++i)
    segmap.insert(keys[i], values[i]);
  end = high_resolution_clock::now();
  auto segMapInsertMs = duration_cast<milliseconds>(end - start).count();

  // Prepare lookup keys: for each real key, append ".extra" to force prefix
  // match
  std::vector<std::string> lookupKeys;
  for (size_t i = 0; i < lookups; ++i) {
    const std::string &base = keys[i % keys.size()];
    lookupKeys.push_back(base + ".extra");
  }

  // Trie prefix search
  size_t foundTriePrefix = 0;
  start = high_resolution_clock::now();
  for (const auto &key : lookupKeys)
    if (trie.findByLongestPrefix(key))
      ++foundTriePrefix;
  end = high_resolution_clock::now();
  auto triePrefixSearchMs = duration_cast<milliseconds>(end - start).count();

  // SegmentMap prefix search
  size_t foundSegMapPrefix = 0;
  start = high_resolution_clock::now();
  for (const auto &key : lookupKeys)
    if (segmap.findByLongestPrefix(key))
      ++foundSegMapPrefix;
  end = high_resolution_clock::now();
  auto segMapPrefixSearchMs = duration_cast<milliseconds>(end - start).count();

  std::cout << "\nPrefix-heavy benchmark results (depth=" << depth
            << ", branches=" << branches << "):\n";
  std::cout << "SegmentTrie:    insert " << trieInsertMs
            << " ms // prefix-search " << triePrefixSearchMs << " ms\n";
  std::cout << "  prefix-found: " << foundTriePrefix << "/" << lookups << "\n";
  std::cout << "SegmentMap:     insert " << segMapInsertMs
            << " ms // prefix-search " << segMapPrefixSearchMs << " ms\n";
  std::cout << "  prefix-found: " << foundSegMapPrefix << "/" << lookups
            << "\n";
}
*/
void benchmarkLoggerSegmentMapTrie(size_t numKeys = 100000,
                                   size_t numLookups = 100000) {
  using namespace std::chrono;
  using lfy::Logger;
  using lfy::SegmentMap;
  using lfy::SegmentTrie;

  // Generate dot-separated logger names
  std::vector<std::string> keys;
  std::mt19937 rng(123);
  std::uniform_int_distribution<int> numSegDist(2, 250);
  std::uniform_int_distribution<int> segLenDist(2, 3);
  std::uniform_int_distribution<int> charDist('a', 'z');
  for (size_t i = 0; i < numKeys; ++i) {
    std::string key;
    int segments = numSegDist(rng);
    for (int s = 0; s < segments; ++s) {
      if (s > 0)
        key += '.';
      int len = segLenDist(rng);
      for (int l = 0; l < len; ++l)
        key += static_cast<char>(charDist(rng));
    }
    keys.push_back(std::move(key));
  }

  // Prepare logger values
  std::vector<std::shared_ptr<Logger>> values;
  for (size_t i = 0; i < numKeys; ++i)
    values.push_back(std::make_shared<Logger>(keys[i]));

  // Benchmark SegmentTrie<Logger> insertion
  SegmentTrie<Logger> trie;
  auto start = high_resolution_clock::now();
  for (size_t i = 0; i < numKeys; ++i)
    trie.insert(keys[i], values[i]);
  auto end = high_resolution_clock::now();
  auto trieInsertMs = duration_cast<milliseconds>(end - start).count();

  // Benchmark SegmentMap insertion
  SegmentMap segmap;
  start = high_resolution_clock::now();
  for (size_t i = 0; i < numKeys; ++i)
    segmap.insert(keys[i], values[i]);
  end = high_resolution_clock::now();
  auto segMapInsertMs = duration_cast<milliseconds>(end - start).count();

  // Prepare random lookup indices
  std::uniform_int_distribution<size_t> idxDist(0, numKeys - 1);
  std::vector<std::string_view> lookupKeys;
  for (size_t i = 0; i < numLookups; ++i)
    lookupKeys.push_back(keys[idxDist(rng)]);

  // Prepare longest prefix match keys: all original keys + all keys with
  // arbitrary postfix segments appended
  std::vector<std::string> prefixLookupKeys;
  prefixLookupKeys.reserve(numLookups * 2);
  std::uniform_int_distribution<int> postfixSegDist(1,
                                                    100); // 1-5 extra segments
  std::uniform_int_distribution<int> postfixLenDist(
      2, 5); // 2-20 chars per segment
  for (size_t i = 0; i < numLookups; ++i) {
    const std::string &base = keys[idxDist(rng)];
    prefixLookupKeys.push_back(base); // original key (exact match)
    // Generate arbitrary postfix
    std::string postfix;
    int numPostfixSegs = postfixSegDist(rng);
    for (int s = 0; s < numPostfixSegs; ++s) {
      postfix += ".";
      int len = postfixLenDist(rng);
      for (int l = 0; l < len; ++l)
        postfix += static_cast<char>(charDist(rng));
    }
    prefixLookupKeys.push_back(base + postfix); // key with arbitrary postfix
  }

  // Benchmark SegmentTrie<Logger> find
  size_t foundTrie = 0;
  auto startFind = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (trie.find(key))
      ++foundTrie;
  auto endFind = high_resolution_clock::now();
  auto trieFindMs = duration_cast<milliseconds>(endFind - startFind).count();

  // Benchmark SegmentMap find
  size_t foundMap = 0;
  auto startMapFind = high_resolution_clock::now();
  for (auto key : lookupKeys)
    if (segmap.find(key))
      ++foundMap;
  auto endMapFind = high_resolution_clock::now();
  auto segMapFindMs =
      duration_cast<milliseconds>(endMapFind - startMapFind).count();

  // Benchmark SegmentTrie<Logger> longest prefix find (with .extended keys)
  size_t foundTriePrefix = 0;
  auto startPrefix = high_resolution_clock::now();
  for (const auto &key : prefixLookupKeys)
    if (trie.findByLongestPrefix(key))
      ++foundTriePrefix;
  auto endPrefix = high_resolution_clock::now();
  auto triePrefixFindMs =
      duration_cast<milliseconds>(endPrefix - startPrefix).count();

  // Benchmark SegmentMap longest prefix find (with .extended keys)
  size_t foundMapPrefix = 0;
  auto startMapPrefix = high_resolution_clock::now();
  for (const auto &key : prefixLookupKeys)
    if (segmap.findByLongestPrefix(key))
      ++foundMapPrefix;
  auto endMapPrefix = high_resolution_clock::now();
  auto segMapPrefixFindMs =
      duration_cast<milliseconds>(endMapPrefix - startMapPrefix).count();

  std::cout << "\nLogger SegmentMap/Trie benchmark results:\n";
  std::cout << "SegmentTrie<Logger>: insert " << trieInsertMs << " ms // find "
            << trieFindMs << " ms // prefix-find " << triePrefixFindMs
            << " ms\n";
  std::cout << "  found:             " << foundTrie << "/" << numLookups
            << " // prefix-found: " << foundTriePrefix << "/" << numLookups
            << "\n";
  std::cout << "SegmentMap:         insert " << segMapInsertMs << " ms // find "
            << segMapFindMs << " ms // prefix-find " << segMapPrefixFindMs
            << " ms\n";
  std::cout << "  found:             " << foundMap << "/" << numLookups
            << " // prefix-found: " << foundMapPrefix << "/" << numLookups
            << "\n";
}

int main() {
  // This is a dummy main function to ensure the code compiles.
  using namespace lfy;
  std::shared_ptr<Logger> logger = Repository::getLogger("dummy_logger");
  logger->addOutputter(std::make_shared<ConsoleOutputter>())
      .addHeaderGenerator(headergen::Time)
      .addHeaderGenerator(headergen::Level)
      .addHeaderGenerator(headergen::LoggerName)
      .setFlusher(flushers::TimeThresholdFlush)
      .addOutputter(std::make_shared<FileOutputter>("output.txt"));

  std::shared_ptr<Logger> childLogger =
      Repository::getLogger("dummy_logger.example", true);
  childLogger->setLogLevel(LogLevel::Warn);

  std::shared_ptr<Logger> childLogger2 =
      Repository::getLogger("dummy_logger.example.child", true);

  std::shared_ptr<Logger> loggerCopy = Repository::getLogger("dummy_logger");

  logger->debug("This is a debug message.");
  logger->info("This is an info message.");
  logger->warn("This is a warning message.");
  logger->error("This is an error message.");

  childLogger->debug("This debug message will not be logged due to log level.");
  childLogger->info("This info message will not be logged due to log level.");
  childLogger->warn("This is a warning message from the child logger.");
  childLogger->error("This is an error message from the child logger.");

  childLogger2->debug(
      "This debug message will not be logged due to log level.");
  childLogger2->info("This info message will not be logged due to log level.");
  childLogger2->warn("This is a warning message from the child logger 2.");
  childLogger2->error("This is an error message from the child logger 2.");

  loggerCopy->debug("This debug message will not be logged due to log level.");
  loggerCopy->info("This is an info message from the copied logger.");
  loggerCopy->warn("This is a warning message from the copied logger.");
  loggerCopy->error("This is an error message from the copied logger.");

  loggerCopy->setLogLevel(LogLevel::Warn);
  loggerCopy->debug("This debug message will not be logged due to log level.");
  loggerCopy->info("This info message will not be logged due to log level.");
  loggerCopy->warn("This is a warning message from the copied logger.");
  loggerCopy->error("This is an error message from the copied logger.");

  logger->debug("This debug message will not be logged due to log level.");
  logger->info("This info message will not be logged due to log level.");
  logger->warn("This is a warning message after changing log level.");
  logger->error("This is an error message after changing log level.");

  /*std::vector<std::jthread> threads;
  for (size_t i = 0; i < 10; ++i)
    threads.emplace_back([i, &logger]() {
      for (size_t j = 0; j < 100; ++j) {
        logger->info("Thread {}: Message {}", i, j);
      }
    });

  // Read and parse a JSON file
  std::ifstream ifs("testfile.txt");
  if (!ifs) {
    logger->error("Failed to open test.json");
    return 1;
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  std::string jsonStr = buffer.str();

  rapidjson::Document doc;
  if (doc.Parse(jsonStr.c_str()).HasParseError()) {
    logger->error("Failed to parse JSON file");
    return 1;
  }

  // Print the JSON as a string
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  doc.Accept(writer);
  logger->info("Parsed JSON: {}", sb.GetString());
  */

  SegmentTrie<std::string> trie;
  trie.insert("com", std::make_shared<std::string>("com_value"));
  trie.insert("com.example",
              std::make_shared<std::string>("com.example_value"));
  trie.insert("com.example.test",
              std::make_shared<std::string>("com.example.test_value"));
  trie.insert("other.example",
              std::make_shared<std::string>("other.example_value"));

  logger->info("Trie test:");
  logger->info("com: {}", *trie.find("com"));
  logger->info("com.example: {}", *trie.find("com.example"));
  logger->info("com.example.test: {}", *trie.find("com.example.test"));
  logger->info("other.example: {}", *trie.find("other.example"));
  logger->info("com.example.noninserted.obj: {}",
               *trie.findByLongestPrefix("com.example.noninserted.obj"));

  trie.remove("com.example.test");
  logger->info("After removing com.example:");
  if (trie.find("com.example.test")) {
    logger->info("com.example still exists");
  } else {
    logger->info("com.example has been removed");
  }

  // benchmarkTrieVsMap();
  // benchmarkTrieVsMapPrefixHeavy();
  benchmarkLoggerSegmentMapTrie();
  return 0;
}