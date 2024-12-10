#include <chrono>
#include <pep/auth/UserGroups.hpp>
#include <pep/networking/NetworkingSerializers.hpp>
#include <pep/server/MonitorableServer.hpp>

#include <pep/utils/Bitpacking.hpp>

#include <prometheus/text_serializer.h>
#include <prometheus/gauge.h>
#include <pep/utils/ApplicationMetrics.hpp>

namespace pep {

MonitorableServerBase::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  uncaughtExceptions_count(prometheus::BuildGauge()
    .Name("pep_uncaughtExceptions_count")
    .Help("Number of uncaught exceptions while dealing with a request")
    .Register(*registry)
    .Add({})),
  diskUsageProportion(prometheus::BuildGauge()
    .Name("pep_diskUsage_ratio")
    .Help("Disk usage percentage for server")
    .Register(*registry)
    .Add({})),
  diskUsageTotal(prometheus::BuildGauge()
    .Name("pep_diskUsage_bytes")
    .Help("Total disk usage for server in bytes")
    .Register(*registry)
    .Add({})),
  memoryUsageProportion(prometheus::BuildGauge()
    .Name("pep_memUsage_ratio")
    .Help("Memory usage percentage for server machine (incl. swap)")
    .Register(*registry)
    .Add({})),
  memoryUsagePhysicalProportion(prometheus::BuildGauge()
    .Name("pep_memUsage_ratio_physical")
    .Help("Physical memory usage percentage for server machine")
    .Register(*registry)
    .Add({})),
  memoryUsageTotal(prometheus::BuildGauge()
    .Name("pep_memUsage_bytes")
    .Help("Memory usage for server process in bytes")
    .Register(*registry)
    .Add({})),
  egcacheRSKGeneration(prometheus::BuildGauge()
    .Name("pep_egcache_rsk_generation")
    .Help("Number of entries added to the RSK Cache")
    .Register(*registry)
    .Add({})),
  egcacheTableGeneration(prometheus::BuildGauge()
    .Name("pep_egcache_table_generation")
    .Help("Number of entries added to the Table Cache")
    .Register(*registry)
    .Add({})),
  egcacheRSKUseCount(prometheus::BuildGauge()
    .Name("pep_egcache_rsk_use")
    .Help("Number of times the RSK Cache was used")
    .Register(*registry)
    .Add({})),
  egcacheTableUseCount(prometheus::BuildGauge()
    .Name("pep_egcache_table_use")
    .Help("Number of times the Table Cache was used")
    .Register(*registry)
    .Add({})),
  uptimeMetric(prometheus::BuildGauge()
    .Name("pep_uptime_seconds")
    .Help("Time since startup in seconds")
    .Register(*registry)
    .Add({}))
{
  startupTime = std::chrono::steady_clock::now();
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
MonitorableServerBase::handleMetricsRequest(
  std::shared_ptr<SignedMetricsRequest> signedRequest) {
  signedRequest->open(getRootCAs(), user_group::Watchdog);
  auto registry = getMetricsRegistry();
  if (registry == nullptr)
    throw Error("Server does not collect metrics");

  std::vector<prometheus::MetricFamily> metrics = registry->Collect();
  prometheus::TextSerializer serializer;
  auto body = serializer.Serialize(metrics);
  return TLSMessageServer::Just(MetricsResponse(body));
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
MonitorableServerBase::handleChecksumChainRequest(
  std::shared_ptr<SignedChecksumChainRequest> signedRequest) {
  auto request = signedRequest->open(getRootCAs(), user_group::Watchdog);
  std::optional<uint64_t> maxCheckpoint;

  if (!request.mCheckpoint.empty()) {
    if (request.mCheckpoint.size() != 8)
      throw Error("checkpoint field should either be 8 bytes or 0");
    maxCheckpoint = UnpackUint64BE(request.mCheckpoint);
  }

  uint64_t checksum, checkpoint;
  computeChecksumChainChecksum(
    request.mName,
    maxCheckpoint,
    checksum,
    checkpoint
  );

  ChecksumChainResponse resp;
  resp.mXorredChecksums = PackUint64BE(checksum);
  resp.mCheckpoint = PackUint64BE(checkpoint);
  return TLSMessageServer::Just(std::move(resp));
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
MonitorableServerBase::handleChecksumChainNamesRequest(
  std::shared_ptr<SignedChecksumChainNamesRequest> signedRequest) {
  signedRequest->open(getRootCAs(), user_group::Watchdog);
  ChecksumChainNamesResponse resp;
  resp.mNames = getChecksumChainNames();
  return TLSMessageServer::Just(std::move(resp));
}

MonitorableServerBase::MonitorableServerBase()
  : mRegistry(std::make_shared<prometheus::Registry>()),
  mMetrics(std::make_shared<Metrics>(mRegistry)) {
}

std::shared_ptr<prometheus::Registry> MonitorableServerBase::getMetricsRegistry() {
  if(mMetrics == nullptr) {
    throw std::runtime_error("Requesting metrics registry on a server that has no metrics.");
  }

  // Collect some metrics ad hoc
  mMetrics->uncaughtExceptions_count.Set(static_cast<double>(getNumberOfUncaughtReadExceptions()));

  auto dataLocation = getStoragePath();

  mMetrics->diskUsageProportion.Set(ApplicationMetrics::GetDiskUsageProportion(dataLocation));
  mMetrics->diskUsageTotal.Set(ApplicationMetrics::GetDiskUsageBytes(dataLocation));

  auto [memoryUsagePhysicalRatio, memoryUsageTotalRatio] = pep::ApplicationMetrics::GetMemoryUsageProportion();
  mMetrics->memoryUsagePhysicalProportion.Set(memoryUsagePhysicalRatio);
  mMetrics->memoryUsageProportion.Set(memoryUsageTotalRatio);
  mMetrics->memoryUsageTotal.Set(pep::ApplicationMetrics::GetMemoryUsageBytes());

  if (!this->mEGCache)
      this->mEGCache = EGCache::get();

  auto egcm = this->mEGCache->getMetrics();

  mMetrics->egcacheRSKGeneration.Set(static_cast<double>(egcm.rsk.generation));
  mMetrics->egcacheTableGeneration.Set(static_cast<double>(egcm.table.generation));
  mMetrics->egcacheRSKUseCount.Set(static_cast<double>(egcm.rsk.useCount));
  mMetrics->egcacheTableUseCount.Set(static_cast<double>(egcm.table.useCount));
  mMetrics->uptimeMetric.Set(std::chrono::duration<double>(std::chrono::steady_clock::now() - mMetrics->startupTime).count()); // in seconds
  return mRegistry;
}

}
