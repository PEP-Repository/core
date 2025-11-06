#include <pep/utils/ApplicationMetrics.hpp>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#elif defined(__linux__)
#include <boost/interprocess/mapped_region.hpp>
#else
#include <pep/utils/Log.hpp>
#endif // _WIN32 or __linux__

#include <cmath>
#include <iostream>
#include <fstream>
#include <atomic>
#include <sstream>
#include <vector>

namespace pep {
  std::atomic<bool> ApplicationMetrics::warningLogged = false;

  //Returns RAM usage in bytes for the current process. Only on Windows or Linux.
  double pep::ApplicationMetrics::GetMemoryUsageBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    ::GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
    return static_cast<double>(pmc.PrivateUsage);
#elif defined(__linux__)
    auto pagesize = boost::interprocess::mapped_region::get_page_size();
    std::ifstream infile("/proc/self/statm");
    if (!infile.is_open()) {
      throw std::runtime_error("Error executing statm command on unix");
    }

    size_t result = 0;
    int ignore{};

    infile >> ignore >> result;

    return static_cast<double>(result * pagesize);
#else
    if (!warningLogged) {
      LOG("ApplicationMetrics", warning) << "Memory usage metrics are not implemented for this platform";
      warningLogged = true;
    }
    return nan("");
#endif // _WIN32 or __linux__
  }

  /// Returns RAM usage percentage for the machine it's running on. Only on Windows or Linux.
  /// First pair entry is physical memory, second entry is total memory (incl. swap)
  std::pair<double, double> pep::ApplicationMetrics::GetMemoryUsageProportion() {
#ifdef _WIN32
    PERFORMANCE_INFORMATION pfInfo;
    pfInfo.cb = sizeof(PERFORMANCE_INFORMATION);
    if (!::GetPerformanceInfo(&pfInfo, pfInfo.cb)) {
      return {nan(""), nan("")};
    }

    double physicalRatio = static_cast<double>(pfInfo.PhysicalTotal - pfInfo.PhysicalAvailable) / static_cast<double>(pfInfo.PhysicalTotal);
    double totalRatio = static_cast<double>(pfInfo.CommitLimit - pfInfo.CommitTotal) / static_cast<double>(pfInfo.CommitLimit);
    return {physicalRatio, totalRatio};
#elif defined(__linux__)
    std::ifstream infile("/proc/meminfo");
    if(!infile.is_open()) {
      throw std::runtime_error("Error executing meminfo command on unix");
    }

    // According to the Linux kernel docs, we should read the contents from /proc/meminfo in a single call to read
    // See https://www.kernel.org/doc/Documentation/filesystems/proc.txt and https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/1869#note_36299
    std::vector<char> buffer(4096);
    infile.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    infile.close();

    std::stringstream ss;
    ss.rdbuf()->pubsetbuf(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    uint64_t memTotal = 0, memAvailable = 0, swapTotal = 0, swapAvailable = 0, readValue = 0;
    std::string readName;
    std::string ignore;

    while(ss >> readName >> readValue >> ignore) {
      if(readName.empty()) {
        break;
      }

      if("MemTotal:" == readName)
        memTotal = readValue;
      else if("MemAvailable:" == readName)
        memAvailable = readValue;
      else if("SwapTotal:" == readName)
        swapTotal = readValue;
      else if("SwapFree:" == readName)
        swapAvailable = readValue;
    }

    if(memTotal == 0 || memAvailable == 0) {
      //If there were problems getting values for RAM usage return nan, swap values are not checked as they may be 0
      return {nan(""), nan("")};
    }

    double physicalRatio = static_cast<double>(memTotal - memAvailable) / static_cast<double>(memTotal);
    double totalRatio = static_cast<double>(memTotal - memAvailable + swapTotal - swapAvailable) / static_cast<double>(memTotal + swapTotal);
    return {physicalRatio, totalRatio};
#else
    if (!warningLogged) {
      LOG("ApplicationMetrics", warning) << "Memory usage metrics are not implemented for this platform";
      warningLogged = true;
    }
    return {nan(""), nan("")};
#endif // _WIN32 or __linux__
  }

  //Returns current disk usage in bytes of the drive on which the folder is located.
  double pep::ApplicationMetrics::GetDiskUsageBytes(std::optional<std::filesystem::path> path) {
    if (!path.has_value()) {
      return nan("");
    }
    std::filesystem::space_info currentSpaceInfo = std::filesystem::space(*path);
    return static_cast<double>(currentSpaceInfo.capacity) - static_cast<double>(currentSpaceInfo.available);
  }

  //Returns current disk usage percentage of the drive on which the folder is located.
  double pep::ApplicationMetrics::GetDiskUsageProportion(std::optional<std::filesystem::path> path) {
    if (!path.has_value()) {
      return nan("");
    }
    std::filesystem::space_info currentSpaceInfo = std::filesystem::space(*path);
    return 1 - (static_cast<double>(currentSpaceInfo.available) / static_cast<double>(currentSpaceInfo.capacity));
  }
}
