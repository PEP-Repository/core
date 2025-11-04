#include <pep/utils/RegisteredTestEnvironment.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Random.hpp>

#ifndef _WIN32
#include <sys/resource.h>
#else
#include <Windows.h>
#include <Psapi.h>
#endif

int main(int argc, char* argv[]) {
  auto env = pep::RegisteredTestEnvironment::Create(argc, argv);
  if (env != nullptr) {
    testing::AddGlobalTestEnvironment(env); // googletest takes ownership of the registered environment objects. No further memory management required. See: http://google.github.io/googletest/advanced.html
  }

  // Ensure that LOG(...) messages are properly formatted, filtered, and sent to a file sink...
  pep::Logging::Initialize({ std::make_shared<pep::FileLogging>(pep::info) });
  // pep::Logging::Initialize({ std::make_shared<pep::ConsoleLogging>(pep::info) }); // ... but not to the console to prevent clutter

  unsigned int seed;
  pep::RandomBytes(reinterpret_cast<uint8_t*>(&seed), sizeof(seed));
  std::srand(seed);

  ::testing::InitGoogleTest(&argc, argv);
  int retval = RUN_ALL_TESTS();

  uint64_t maxmem;
#ifndef _WIN32
  rusage usage{};
  ::getrusage(RUSAGE_SELF, &usage);
  maxmem = static_cast<uint64_t>(usage.ru_maxrss);
#if defined(__APPLE__) && defined(__MACH__)
  maxmem /= 1024;
#endif
#else
  PROCESS_MEMORY_COUNTERS pmc;
  if (!::GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    std::cerr << "Could not read memory info" << std::endl;
    exit(2);
  }
  maxmem = pmc.PeakWorkingSetSize / 1024;
#endif
  std::cout << maxmem << " kilobytes of memory used at max" << std::endl;
  std::cout << (maxmem / 1024) << " megabytes of memory used at max" << std::endl;

  return retval;
}
