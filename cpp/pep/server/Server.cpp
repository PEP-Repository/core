#include <chrono>
#include <pep/auth/UserGroup.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/server/Server.hpp>

#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>

#include <prometheus/text_serializer.h>
#include <prometheus/gauge.h>
#include <pep/utils/ApplicationMetrics.hpp>

namespace pep {

Server::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
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

messaging::MessageBatches
Server::handleMetricsRequest(
  std::shared_ptr<SignedMetricsRequest> signedRequest) {
  signedRequest->validate(*getRootCAs(), UserGroup::Watchdog);
  auto registry = getMetricsRegistry();
  if (registry == nullptr)
    throw Error("Server does not collect metrics");

  std::vector<prometheus::MetricFamily> metrics = registry->Collect();
  prometheus::TextSerializer serializer;
  auto body = serializer.Serialize(metrics);
  return messaging::BatchSingleMessage(MetricsResponse(body));
}

std::unordered_set<std::string> Server::getAllowedChecksumChainRequesters() {
  return { UserGroup::Watchdog };
}


messaging::MessageBatches
Server::handleChecksumChainRequest(
  std::shared_ptr<SignedChecksumChainRequest> signedRequest) {
  UserGroup::EnsureAccess(getAllowedChecksumChainRequesters(), signedRequest->getLeafCertificateOrganizationalUnit(), "Requesting checksum chains");
  auto request = signedRequest->open(*getRootCAs());
  std::optional<uint64_t> maxCheckpoint;

  if (!request.mCheckpoint.empty()) {
    if (request.mCheckpoint.size() != 8)
      throw Error("checkpoint field should either be 8 bytes or 0");
    maxCheckpoint = UnpackUint64BE(request.mCheckpoint);
  }

  uint64_t checksum{}, checkpoint{};
  computeChecksumChainChecksum(
    request.mName,
    maxCheckpoint,
    checksum,
    checkpoint
  );

  ChecksumChainResponse resp;
  resp.mXorredChecksums = PackUint64BE(checksum);
  resp.mCheckpoint = PackUint64BE(checkpoint);
  return messaging::BatchSingleMessage(std::move(resp));
}

messaging::MessageBatches
Server::handleChecksumChainNamesRequest(
  std::shared_ptr<SignedChecksumChainNamesRequest> signedRequest) {
  UserGroup::EnsureAccess(getAllowedChecksumChainRequesters(), signedRequest->getLeafCertificateOrganizationalUnit(), "Requesting checksum chain names");
  signedRequest->validate(*getRootCAs());
  ChecksumChainNamesResponse resp;
  resp.mNames = getChecksumChainNames();
  return messaging::BatchSingleMessage(std::move(resp));
}

Server::Server(std::shared_ptr<Parameters> parameters)
  : mRegistry(std::make_shared<prometheus::Registry>()),
  mMetrics(std::make_shared<Metrics>(mRegistry)),
  mEGCache(EGCache::get()),
  mDescription(parameters->serverTraits().description()),
  mIoContext(parameters->getIoContext()),
  mRootCAs(parameters->ensureValid().getRootCAs()) {
  RegisterRequestHandlers(*this,
    &Server::handleMetricsRequest,
    &Server::handleChecksumChainNamesRequest,
    &Server::handleChecksumChainRequest);
}

std::filesystem::path Server::EnsureDirectoryPath(std::filesystem::path path) {
  return path / "";
}

std::shared_ptr<prometheus::Registry> Server::getMetricsRegistry() {
  if(mMetrics == nullptr) {
    throw std::runtime_error("Requesting metrics registry on a server that has no metrics.");
  }

  // Collect some metrics ad hoc
  mMetrics->uncaughtExceptions_count.Set(static_cast<double>(getNumberOfUncaughtReadExceptions()));

  auto dataLocation = getStoragePath();

  // Will be NaN for servers without dataLocation
  mMetrics->diskUsageProportion.Set(ApplicationMetrics::GetDiskUsageProportion(dataLocation));
  mMetrics->diskUsageTotal.Set(ApplicationMetrics::GetDiskUsageBytes(dataLocation));

  auto [memoryUsagePhysicalRatio, memoryUsageTotalRatio] = pep::ApplicationMetrics::GetMemoryUsageProportion();
  mMetrics->memoryUsagePhysicalProportion.Set(memoryUsagePhysicalRatio);
  mMetrics->memoryUsageProportion.Set(memoryUsageTotalRatio);
  mMetrics->memoryUsageTotal.Set(pep::ApplicationMetrics::GetMemoryUsageBytes());

  auto egcm = this->getEgCache().getMetrics();

  mMetrics->egcacheRSKGeneration.Set(static_cast<double>(egcm.rsk.generation));
  mMetrics->egcacheTableGeneration.Set(static_cast<double>(egcm.table.generation));
  mMetrics->egcacheRSKUseCount.Set(static_cast<double>(egcm.rsk.useCount));
  mMetrics->egcacheTableUseCount.Set(static_cast<double>(egcm.table.useCount));
  mMetrics->uptimeMetric.Set(std::chrono::duration<double>(std::chrono::steady_clock::now() - mMetrics->startupTime).count()); // in seconds
  return mRegistry;
}

Server::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> ioContext, const Configuration& config)
  : mIoContext(std::move(ioContext)),
  rootCACertificatesFilePath_(config.get<std::filesystem::path>("CACertificateFile")),
  rootCAs_(std::make_shared<X509RootCertificates>(ReadFile(rootCACertificatesFilePath_))) {}

}
