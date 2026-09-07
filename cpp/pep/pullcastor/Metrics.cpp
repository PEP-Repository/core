#include <pep/utils/File.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/pullcastor/Metrics.hpp>

#include <prometheus/text_serializer.h>

namespace pep {
namespace castor {

Metrics::Metrics()
  : Metrics("Unconfigured job", std::nullopt) {
}

Metrics::Metrics(const std::string& jobname, const std::filesystem::path& metricsFile)
  : Metrics(jobname, std::optional<std::filesystem::path>(metricsFile)) {
}

Metrics::Metrics(const std::string& jobname, const std::optional<std::filesystem::path>& metricsFile)
  : RegisteredMetrics(std::make_shared<prometheus::Registry>()),
  uncaughtExceptionsCount(prometheus::BuildCounter()
    .Name("pep_uncaughtExceptions_count")
    .Labels({{"job", jobname}})
    .Help("Number of unhandled errors during the last Castor import")
    .Register(*getRegistry())
    .Add({})),
  storedEntriesCount(prometheus::BuildCounter()
    .Name("pep_storedEntries_count")
    .Labels({{"job", jobname}})
    .Help("Number of entries stored in PEP in the last Castor import")
    .Register(*getRegistry())
    .Add({})),
  importDurationSeconds(prometheus::BuildGauge()
    .Name("pep_importDuration_seconds")
    .Labels({{"job", jobname}})
    .Help("Duration in seconds of the last Castor import")
    .Register(*getRegistry())
    .Add({})),
  importTimestampSeconds(prometheus::BuildGauge()
    .Name("pep_importTimestamp_seconds")
    .Labels({{"job", jobname}})
    .Help("Unix Timestamp of the last Castor import")
    .Register(*getRegistry())
    .Add({})),
    metricsFile_(metricsFile)
{ }

Metrics::~Metrics() noexcept {
  try {
    importTimestampSeconds.SetToCurrentTime();

    std::vector<prometheus::MetricFamily> metrics = this->getRegistry()->Collect();

    if (metricsFile_.has_value()) {
      std::filesystem::path tmpPath = *metricsFile_;
      tmpPath += ".$$";

      prometheus::TextSerializer serializer;
      WriteFile(tmpPath, serializer.Serialize(metrics));

      std::filesystem::rename(tmpPath, *metricsFile_); //Atomic write, as per https://github.com/prometheus/node_exporter#textfile-collector
    }
  }
  catch (...) {
    PEP_LOG("PullCastor", Severity::Error) << "Error writing metrics: " << GetExceptionMessage(std::current_exception());
  }
}

}
}
