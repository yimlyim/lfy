#include "lfy/LogRepository.hpp"
#include "lfy/Logger.hpp"
#include "lfy/Outputter.hpp"
#include <cstddef>
#include <fstream>
#include <sstream>
#include <thread>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

int main() {
  // This is a dummy main function to ensure the code compiles.
  using namespace lfy;
  std::shared_ptr<Logger> logger = LogRepository{}.getLogger("dummy_logger");
  logger->addOutputter(std::make_unique<ConsoleOutputter>())
      .addHeaderGenerator(headergen::Time)
      .addHeaderGenerator(headergen::Level);

  logger->debug("This is a debug message.");
  logger->info("This is an info message.");
  logger->warn("This is a warning message.");
  logger->error("This is an error message.");

  std::vector<std::jthread> threads;
  for (size_t i = 0; i < 10; ++i)
    threads.emplace_back([i, &logger]() {
      logger->info("Logging from thread {}", i);
      logger->warn("Logging from thread {}", i);
      logger->info("Logging from thread {}", i);
      logger->info("Logging from thread {}", i);
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

  return 0;
}