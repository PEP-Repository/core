#pragma once

#include <pep/metrics/RegisteredMetrics.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/networking/TLSMessageServer.hpp>
#include <pep/server/MonitoringSerializers.hpp>

namespace pep {

class MonitorableServerBase {
protected:
  std::shared_ptr<prometheus::Registry> mRegistry;

  MonitorableServerBase();
  virtual ~MonitorableServerBase() = default;

  virtual std::shared_ptr<prometheus::Registry> getMetricsRegistry();

  // Used to create ChecksumChainNamesResponse
  virtual std::vector<std::string> getChecksumChainNames() const {
    return {};
  }

  // Used to create ChecksumChainResponse
  virtual void computeChecksumChainChecksum(const std::string& chain, // name of chain to compute checksum of
                                                                      // compute value for the given checkpoint
                                            std::optional<uint64_t> maxCheckpoint,
                                            uint64_t& checksum,  // out: computed checksum
                                            uint64_t& checkpoint // out: current checkpoint
  ) {
    throw Error("Does not support checksum chains");
  }

  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleMetricsRequest(std::shared_ptr<SignedMetricsRequest> signedRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleChecksumChainNamesRequest(std::shared_ptr<SignedChecksumChainNamesRequest> signedRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleChecksumChainRequest(std::shared_ptr<SignedChecksumChainRequest> signedRequest);

  virtual const X509RootCertificates& getRootCAs() const noexcept = 0;
  virtual unsigned int getNumberOfUncaughtReadExceptions() const noexcept = 0;
  virtual std::optional<std::filesystem::path> getStoragePath() = 0;

private:
  class Metrics : public RegisteredMetrics {
  public:
    Metrics(std::shared_ptr<prometheus::Registry> registry);

    std::chrono::steady_clock::time_point startupTime;

    prometheus::Gauge& uncaughtExceptions_count;

    prometheus::Gauge& diskUsageProportion;
    prometheus::Gauge& diskUsageTotal;

    // Percentage of total memory used (physical + swap)
    prometheus::Gauge& memoryUsageProportion;

    // Percentage of physical memory used
    prometheus::Gauge& memoryUsagePhysicalProportion;

    // Total memory usage, in bytes
    prometheus::Gauge& memoryUsageTotal;

    prometheus::Gauge& egcacheRSKGeneration;
    prometheus::Gauge& egcacheTableGeneration;
    prometheus::Gauge& egcacheRSKUseCount;
    prometheus::Gauge& egcacheTableUseCount;

    prometheus::Gauge& uptimeMetric;
  };

  std::shared_ptr<Metrics> mMetrics;
  std::shared_ptr<EGCache> mEGCache; // <- for metrics
};


template <typename TBase>
class MonitorableServer : public TBase, public MonitorableServerBase {
  static_assert(std::is_base_of<TLSMessageServer, TBase>::value);

private:
  // Without these (IMO redundant) forwarding handleXyzRequest methods, the constructor's
  // invocation of TBase::registerRequestHandlers fails to compile (using the Microsoft compiler).
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleMetricsRequest(std::shared_ptr<SignedMetricsRequest> signedRequest) {
    return MonitorableServerBase::handleMetricsRequest(signedRequest);
  }

  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleChecksumChainNamesRequest(std::shared_ptr<SignedChecksumChainNamesRequest> signedRequest) {
    return MonitorableServerBase::handleChecksumChainNamesRequest(signedRequest);
  }

  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleChecksumChainRequest(std::shared_ptr<SignedChecksumChainRequest> signedRequest) {
    return MonitorableServerBase::handleChecksumChainRequest(signedRequest);
  }

protected:
  MonitorableServer(std::shared_ptr<typename TBase::Parameters> parameters)
    : TBase(parameters) {
    TBase::registerRequestHandlers(this,
      &MonitorableServer::handleMetricsRequest,
      &MonitorableServer::handleChecksumChainNamesRequest,
      &MonitorableServer::handleChecksumChainRequest);
  }

  const X509RootCertificates& getRootCAs() const noexcept override { return TBase::getRootCAs(); }
  unsigned int getNumberOfUncaughtReadExceptions() const noexcept override { return TBase::getNumberOfUncaughtReadExceptions(); }
  std::optional<std::filesystem::path> getStoragePath() override { return TBase::getStoragePath(); }
};

}
