#include <pep/utils/ApplicationMetrics.hpp>
#include <gtest/gtest.h>

namespace {

TEST(ApplicationMetrics, GetMemoryUsageBytes) {
#if !(defined(_WIN32) || defined(__linux__))
  GTEST_SKIP() << "GetMemoryUsageBytes test is unavailable on this platform.";
#endif

  // ApplicationMetrics::GetMemoryUsageBytes() returns 0 if called with Address Sanitizer enabled
  ASSERT_GE(pep::ApplicationMetrics::GetMemoryUsageBytes(), 0);
  ASSERT_LT(pep::ApplicationMetrics::GetMemoryUsageBytes(), static_cast<double>(uint64_t{1 << 30}) * 1024); // 1024 GB...
}

TEST(ApplicationMetrics, GetMemoryUsageProportion) {
#if !(defined(_WIN32) || defined(__linux__))
  GTEST_SKIP() << "GetMemoryUsageProportion test is unavailable on this platform.";
#else
  auto memory = pep::ApplicationMetrics::GetMemoryUsageProportion();
  ASSERT_GT(memory.first, 0.0);
  ASSERT_GT(memory.second, 0.0);

  ASSERT_LE(memory.first, 1.0);
  ASSERT_LE(memory.second, 1.0);
#endif
}

TEST(ApplicationMetrics, GetDiskUsageBytes) {
  auto diskUsage = pep::ApplicationMetrics::GetDiskUsageBytes("/");
  ASSERT_GT(diskUsage, 0.0);
}

TEST(ApplicationMetrics, GetDiskUsageProportion) {
  auto diskUsage = pep::ApplicationMetrics::GetDiskUsageProportion("/");
  ASSERT_GT(diskUsage, 0.0);
  ASSERT_LT(diskUsage, 1.0);
}

}
