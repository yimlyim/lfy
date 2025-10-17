#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <random>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include "lfy/Format.hpp"
#include "lfy/Logger.hpp"
#include "lfy/Outputter.hpp"
#include "lfy/Repository.hpp"

int main() {
  // This is a dummy main function to ensure the code compiles.
  using namespace lfy;
  using namespace lfy::literals;
  using namespace std::chrono_literals;

  auto defaultLogger = Repository::getDefaultLogger();
  defaultLogger->addOutputter(outputters::File(16_MiB, "output.txt"))
      .setFlusher(flushers::NeverFlush());
  defaultLogger->seal();

  std::shared_ptr<Logger> logger =
      Repository::getLogger("dummy_logger", Inheritance::Enabled);

  defaultLogger->error("This is an error message from the default logger.");

  logger->debug("This is a debug message.");
  logger->info("This is an info message.");
  logger->warn("This is a warning message.");
  logger->error("This is an error message.");

  std::chrono::time_point start = std::chrono::system_clock::now();
  const std::string msg =
      R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vestibulum pharetra
metus cursus lacus placerat congue. Nulla egestas, mauris a tincidunt tempus, enim lectus volutpat mi,
eu consequat sem libero nec massa. In dapibus ipsum a diam rhoncus gravida. Etiam non dapibus eros.
Donec fringilla dui sed augue pretium, nec scelerisque est maximus. Nullam convallis, sem nec blandit maximus,
nisi turpis ornare nisl, sit amet volutpat neque massa eu odio. Maecenas malesuada quam ex, posuere congue nibh turpis duis.)";

  for (int i = 0; i < 1000000; ++i)
    logger->info("{} {}", msg, i);

  std::chrono::time_point end = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  logger->info("Elapsed time for logging: {:.4f} seconds", elapsed.count());
  std::cerr << "Elapsed time for logging: " << elapsed.count() << " seconds\n";

  return 0;
}