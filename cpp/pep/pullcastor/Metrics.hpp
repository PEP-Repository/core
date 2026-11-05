#pragma once

#include <pep/metrics/RegisteredMetrics.hpp>

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <filesystem>
#include <optional>
#include <string>

namespace pep {
namespace castor {

class Metrics : public RegisteredMetrics {
private:
  Metrics(const std::string& jobname, const std::optional<std::filesystem::path>& metricsFile);

public:
  explicit Metrics();
  Metrics(const std::string& jobname, const std::filesystem::path& metricsFile);
  ~Metrics() noexcept;

  prometheus::Counter& uncaughtExceptionsCount;
  prometheus::Counter& storedEntriesCount;
  prometheus::Gauge& importDurationSeconds;
  prometheus::Gauge& importTimestampSeconds;

private:
  std::optional<std::filesystem::path> metricsFile_;
};

}
}
