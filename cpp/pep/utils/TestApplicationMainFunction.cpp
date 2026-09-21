#include <pep/utils/ApplicationMetrics.hpp>
#include <pep/utils/RegisteredTestEnvironment.hpp>
#include <pep/utils/Log.hpp>

#include <cmath>

int main(int argc, char* argv[]) {
  std::span<const char* const> args(argv, static_cast<std::size_t>(argc));
  auto env = pep::RegisteredTestEnvironment::Create(args);
  if (env != nullptr) {
    testing::AddGlobalTestEnvironment(env); // googletest takes ownership of the registered environment objects. No further memory management required. See: http://google.github.io/googletest/advanced.html
  }

  // Ensure that PEP_LOG(...) messages are properly formatted, filtered, and sent to a file sink...
  pep::Logging::Initialize({ std::make_shared<pep::FileLogging>(pep::Severity::Info) });
  // pep::Logging::Initialize({ std::make_shared<pep::ConsoleLogging>(pep::Severity::Info) }); // ... but not to the console to prevent clutter

  ::testing::InitGoogleTest(&argc, argv);
  int retval = RUN_ALL_TESTS();

  if (const auto maxmem = pep::ApplicationMetrics::GetMemoryUsageBytes();
      !std::isnan(maxmem)) {
    std::cout << (maxmem / (1024 * 1024)) << " megabytes of memory used at max" << std::endl;
  }

  return retval;
}
