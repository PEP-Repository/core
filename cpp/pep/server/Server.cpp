#include <chrono>
#include <filesystem>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>
#include <pep/auth/UserGroup.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/server/Server.hpp>

#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Shared.hpp>

#include <prometheus/text_serializer.h>
#include <prometheus/gauge.h>
#include <prometheus/metric_family.h>
#include <pep/utils/ApplicationMetrics.hpp>
#include <pep/crypto/X509Certificate.hpp> // for X509RootCertificates
#include <pep/messaging/MessageSequence.hpp>
#include <pep/metrics/RegisteredMetrics.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/server/MonitoringMessages.hpp> // for SignedMetricsRequest, MetricsResponse, ChecksumChainResponse
#include <pep/async/IoContext_fwd.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

namespace {

constexpr size_t FormattedChecksumChainValueLength = 8U;

std::string ChecksumChainValueToString(uint64_t value) {
  auto result = PackUint64BE(value);
  assert(result.size() == FormattedChecksumChainValueLength);
  return result;
}

std::optional<uint64_t> ChecksumChainValueFromString(const std::string& str) {
  std::optional<uint64_t> result;
  if (!str.empty()) {
    if (str.size() != FormattedChecksumChainValueLength) {
      throw Error("checkpoint field should either be " + std::to_string(FormattedChecksumChainValueLength) + " bytes or 0");
    }
    result = UnpackUint64BE(str);
  }
  return result;
}

}


Server::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  uncaughtExceptionsCount(prometheus::BuildGauge()
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
  auto signatory = signedRequest->validate(*getRootCAs());
  UserGroup::EnsureAccess({ UserGroup::Watchdog }, signatory.organizationalUnit(), "Metrics retrieval");
  auto registry = getMetricsRegistry();
  if (registry == nullptr)
    throw Error("Server does not collect metrics");

  std::vector<prometheus::MetricFamily> metrics = registry->Collect();
  prometheus::TextSerializer serializer;
  auto body = serializer.Serialize(metrics);
  return messaging::BatchSingleMessage(MetricsResponse(body));
}

messaging::MessageBatches
Server::handleChecksumChainRequest(
  std::shared_ptr<SignedChecksumChainRequest> signedRequest) {

  auto certified = signedRequest->open(*getRootCAs());
  UserGroup::EnsureAccess({ UserGroup::Watchdog }, certified.signatory.organizationalUnit(), "Requesting checksum chains");

  const auto& request = certified.message;
  auto values = this->checksumChains().chain(request.name).get(ChecksumChainValueFromString(request.checkpoint));

  return messaging::BatchSingleMessage(
    ChecksumChainResponse{
      .xorredChecksums = ChecksumChainValueToString(values.checksum),
      .checkpoint = ChecksumChainValueToString(values.checkpoint),
    });
}

messaging::MessageBatches
Server::handleChecksumChainNamesRequest(
  std::shared_ptr<SignedChecksumChainNamesRequest> signedRequest) {

  auto signatory = signedRequest->validate(*getRootCAs());
  UserGroup::EnsureAccess({ UserGroup::Watchdog }, signatory.organizationalUnit(), "Requesting checksum chain names");

  ChecksumChainNamesResponse resp;
  resp.names = this->checksumChains().names() | std::ranges::to<std::vector>();
  return messaging::BatchSingleMessage(std::move(resp));
}

Server::Server(std::shared_ptr<Parameters> parameters)
  : registry_(std::make_shared<prometheus::Registry>()),
  metrics_(std::make_shared<Metrics>(registry_)),
  eGCache_(EGCache::get()),
  serverTraits_(parameters->serverTraits()),
  ioContext_(parameters->getIoContext()),
  rootCAs_(parameters->ensureValid().getRootCAs()) {
  RegisterRequestHandlers(*this,
    &Server::handleMetricsRequest,
    &Server::handleChecksumChainNamesRequest,
    &Server::handleChecksumChainRequest);
}

std::filesystem::path Server::EnsureDirectoryPath(std::filesystem::path path) {
  return path / "";
}

std::shared_ptr<prometheus::Registry> Server::getMetricsRegistry() {
  if(metrics_ == nullptr) {
    throw std::runtime_error("Requesting metrics registry on a server that has no metrics.");
  }

  // Collect some metrics ad hoc
  metrics_->uncaughtExceptionsCount.Set(static_cast<double>(getNumberOfUncaughtReadExceptions()));

  auto dataLocation = getStoragePath();

  // Will be NaN for servers without dataLocation
  metrics_->diskUsageProportion.Set(ApplicationMetrics::GetDiskUsageProportion(dataLocation));
  metrics_->diskUsageTotal.Set(ApplicationMetrics::GetDiskUsageBytes(dataLocation));

  auto [memoryUsagePhysicalRatio, memoryUsageTotalRatio] = pep::ApplicationMetrics::GetMemoryUsageProportion();
  metrics_->memoryUsagePhysicalProportion.Set(memoryUsagePhysicalRatio);
  metrics_->memoryUsageProportion.Set(memoryUsageTotalRatio);
  metrics_->memoryUsageTotal.Set(pep::ApplicationMetrics::GetMemoryUsageBytes());

  auto egcm = this->getEgCache().getMetrics();

  metrics_->egcacheRSKGeneration.Set(static_cast<double>(egcm.rsk.generation));
  metrics_->egcacheTableGeneration.Set(static_cast<double>(egcm.table.generation));
  metrics_->egcacheRSKUseCount.Set(static_cast<double>(egcm.rsk.useCount));
  metrics_->egcacheTableUseCount.Set(static_cast<double>(egcm.table.useCount));
  metrics_->uptimeMetric.Set(std::chrono::duration<double>(std::chrono::steady_clock::now() - metrics_->startupTime).count()); // in seconds
  return registry_;
}

Server::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> ioContext, const Configuration& config)
  : ioContext_(std::move(ioContext)),
  rootCACertificatesFilePath_(config.get<std::filesystem::path>("CaCertificateFile")),
  rootCAs_(MakeSharedCopy(X509RootCertificates::FromFile(rootCACertificatesFilePath_))) {}

}
