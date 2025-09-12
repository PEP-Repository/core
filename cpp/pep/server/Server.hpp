#pragma once

#include <pep/utils/File.hpp>
#include <pep/messaging/RequestHandler.hpp>
#include <pep/metrics/RegisteredMetrics.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/async/IoContext_fwd.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>

namespace pep {

/*!
* \brief A facility that handles requests. Base class for PEP's various server types.
*/
class Server : public messaging::RequestHandler, public std::enable_shared_from_this<Server> {
public:
  virtual ~Server() = default;

  /*!
  * \brief Parameters required by the server.
  */
  class Parameters;

  /*!
  * \brief Produces a human-readable description of the server.
  * \return A string describing the server (type).
  */
  virtual std::string describe() const = 0;

  /*!
  * \brief Produces the path where the server stores its data (if any).
  * \return A path to the server's (primary) storage location, or std::nullopt if the server doesn't store data.
  */
  virtual std::optional<std::filesystem::path> getStoragePath() { return std::nullopt; }

  /*!
  * \brief Produces the number of uncaught (read) exceptions encountered by the server('s network exposure).
  * \return The number of uncaught (read) exceptions.
  */
  unsigned int getNumberOfUncaughtReadExceptions() const noexcept { return mUncaughtReadExceptions; }

  /*!
  * \brief Registers an uncaught (read) exception encountered by the server('s network exposure).
  */
  void registerUncaughtReadException(std::exception_ptr) { ++mUncaughtReadExceptions; }

protected:
  std::shared_ptr<prometheus::Registry> mRegistry;

  Server(std::shared_ptr<Parameters> parameters);

  virtual std::string makeSerializedPingResponse(const PingRequest& request) const;

  virtual std::shared_ptr<prometheus::Registry> getMetricsRegistry();

  virtual std::unordered_set<std::string> getAllowedChecksumChainRequesters();

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

  const std::shared_ptr<boost::asio::io_context>& getIoContext() const noexcept { return mIoContext; }
  const X509RootCertificates& getRootCAs() const noexcept { return mRootCAs; }
  EGCache& getEgCache() { return mEGCache; }

  static std::filesystem::path EnsureDirectoryPath(std::filesystem::path);

private:
  messaging::MessageBatches handlePingRequest(std::shared_ptr<PingRequest> request);
  messaging::MessageBatches handleMetricsRequest(std::shared_ptr<SignedMetricsRequest> signedRequest);
  messaging::MessageBatches handleChecksumChainNamesRequest(std::shared_ptr<SignedChecksumChainNamesRequest> signedRequest);
  messaging::MessageBatches handleChecksumChainRequest(std::shared_ptr<SignedChecksumChainRequest> signedRequest);

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
  EGCache& mEGCache; // <- for metrics
  unsigned int mUncaughtReadExceptions = 0;
  std::shared_ptr<boost::asio::io_context> mIoContext;
  X509RootCertificates mRootCAs;
};


/*!
* \brief Parameters required by the server.
*/
class Server::Parameters {
private:
  std::shared_ptr<boost::asio::io_context> mIoContext;
  std::filesystem::path rootCACertificatesFilePath_;
  X509RootCertificates rootCAs_;

protected:
  virtual void check() const {}

public:
  /*!
  * \brief Constructor.
  * \param ioContext The I/O context associated with the server.
  * \param config The configuration for the server.
  */
  Parameters(std::shared_ptr<boost::asio::io_context> ioContext, const Configuration& config);

  /*!
   * \brief Destructor.
   */
  virtual ~Parameters() noexcept = default;

  /*!
   * \brief Validates these parameters, raising an exception if they're not valid.
   * \return A reference to this instance.
   */
  const Parameters& ensureValid() const {
    this->check();
    return *this;
  }

  /*!
   * \brief Produces the I/O context associated with this server.
   * \return The I/O context associated with this server.
   */
  const std::shared_ptr<boost::asio::io_context>& getIoContext() const noexcept { return mIoContext; }

  /*!
   * \brief Produces the path to the file containing the root CA certificate(s) for this server's constellation.
   * \return The path to the file containing this server's constellation's root CA certificate(s).
   */
  const std::filesystem::path& getRootCACertificatesFilePath() const noexcept { return rootCACertificatesFilePath_; }

  /*!
   * \brief Produces the root CA certificate(s) for this server's constellation.
   * \return (A reference to) this server's constellation's root CA certificate(s).
   */
  const X509RootCertificates& getRootCAs() const noexcept { return rootCAs_; }
};

}
