#pragma once

#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/key-components/KeyComponentServer.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/rsk-pep/DataTranslator.hpp>
#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

#include <pep/transcryptor/Storage.hpp>

#include <optional>

namespace pep {

class Transcryptor : public KeyComponentServer {
 private:
  class Metrics : public RegisteredMetrics {
   public:
    Metrics(std::shared_ptr<prometheus::Registry> registry);
    prometheus::Summary& transcryptor_request_duration;
    prometheus::Gauge& transcryptor_log_size;
  };

 public:
  class Parameters : public KeyComponentServer::Parameters {
  protected:
    ServerTraits serverTraits() const noexcept override { return ServerTraits::Transcryptor(); }

  public:
    Parameters(
        std::shared_ptr<boost::asio::io_context> io_context,
        const Configuration& config);

    std::shared_ptr<TranscryptorStorage> getStorage() const;
    void setStorage(std::shared_ptr<TranscryptorStorage> storage);

    const ServerVerifiers& getVerifiers() const;
    void setVerifiers(const ServerVerifiers& verifiers);

    std::optional<ElgamalPrivateKey> getPseudonymKey() const;
    void setPseudonymKey(const ElgamalPrivateKey& key);

    const EndPoint& getAccessManagerEndPoint() const { return accessManagerEndPoint; }

   protected:
    void check() const override;

   private:
    std::optional<ElgamalPrivateKey> pseudonymKey;
    std::shared_ptr<TranscryptorStorage> storage;
    std::optional<ServerVerifiers> verifiers;

    EndPoint accessManagerEndPoint;
  };

public:
  explicit Transcryptor(std::shared_ptr<Parameters> parameters);

protected:
  std::optional<std::filesystem::path> getStoragePath() override;
  std::shared_ptr<prometheus::Registry> getMetricsRegistry() override;
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
    uint64_t& checkpoint) override;

private:
  messaging::MessageBatches handleTranscryptorRequest(std::shared_ptr<TranscryptorRequest> pRequest, messaging::MessageSequence entriesObservable);
  messaging::MessageBatches handleRekeyRequest(std::shared_ptr<RekeyRequest> pRequest);
  messaging::MessageBatches handleLogIssuedTicketRequest(std::shared_ptr<LogIssuedTicketRequest> request);

  static std::string ComputePseudonymHash(
    const std::vector<LocalPseudonyms>& pseuds);

private:
  std::shared_ptr<WorkerPool> workerPool_;
  std::optional<ElgamalPrivateKey> pseudonymKey_;
  AccessManagerProxy mAccessManagerProxy;
  std::shared_ptr<TranscryptorStorage> storage_;
  std::shared_ptr<Metrics> lpMetrics;
  ServerVerifiers mVerifiers;
  uintmax_t mNextTranscryptorRequestNumber = 1U;
  uintmax_t mNextLogIssuedTicketRequestNumber = 1U;
};

}
