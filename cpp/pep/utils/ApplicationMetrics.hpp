#pragma once

#include <filesystem>
#include <optional>
#include <atomic>

namespace pep {
  class ApplicationMetrics {
  private:
    static std::atomic<bool> warningLogged;
  public:
    static double GetMemoryUsageBytes();

    // Returns RAM usage percentage for the machine it's running on. Only on Windows or Linux.
    // First pair entry is physical memory, second entry is total memory (incl. swap)
    static std::pair<double, double> GetMemoryUsageProportion();

    static double GetDiskUsageBytes(std::optional<std::filesystem::path> path);
    static double GetDiskUsageProportion(std::optional<std::filesystem::path> path);
  };
}

