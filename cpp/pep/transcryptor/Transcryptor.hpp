#pragma once

#include <pep/async/WorkerPool.hpp>
#include <pep/rsk-pep/DataTranslator.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/server/Server.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

#include <pep/transcryptor/Storage.hpp>

namespace pep {

class Transcryptor : public SigningServer {
 private:
  class Metrics : public RegisteredMetrics {
   public:
    Metrics(std::shared_ptr<prometheus::Registry> registry);
    prometheus::Summary& keyComponent_request_duration;
    prometheus::Summary& transcryptor_request_duration;
    prometheus::Gauge& transcryptor_log_size;
  };

 public:
  class Parameters : public SigningServer::Parameters {
   public:
    Parameters(
        std::shared_ptr<boost::asio::io_service> io_service,
        const Configuration& config);

    std::shared_ptr<PseudonymTranslator> getPseudonymTranslator() const;
    std::shared_ptr<DataTranslator> getDataTranslator() const;
    void setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator);
    void setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator);

    std::shared_ptr<TranscryptorStorage> getStorage() const;
    void setStorage(std::shared_ptr<TranscryptorStorage> storage);

    const VerifiersResponse& getVerifiers() const;
    void setVerifiers(const VerifiersResponse& verifiers);

    boost::optional<ElgamalPrivateKey> getPseudonymKey() const;
    void setPseudonymKey(const ElgamalPrivateKey& key);

   protected:
    void check() override;

   private:
    boost::optional<ElgamalPrivateKey> pseudonymKey;
    std::shared_ptr<PseudonymTranslator> pseudonymTranslator;
    std::shared_ptr<DataTranslator> dataTranslator;
    std::shared_ptr<TranscryptorStorage> storage;
    boost::optional<VerifiersResponse> verifiers;
  };

public:
  explicit Transcryptor(std::shared_ptr<Parameters> parameters);

protected:
  std::string describe() const override;
  std::optional<std::filesystem::path> getStoragePath() override;
  std::shared_ptr<prometheus::Registry> getMetricsRegistry() override;
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
    uint64_t& checkpoint) override;

private:
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> pRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleTranscryptorRequest(std::shared_ptr<TranscryptorRequest> pRequest, rxcpp::observable<std::shared_ptr<std::string>> entriesObservable);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleRekeyRequest(std::shared_ptr<RekeyRequest> pRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleLogIssuedTicketRequest(std::shared_ptr<LogIssuedTicketRequest> request);

  static std::string computePseudonymHash(
    const std::vector<LocalPseudonyms>& pseuds);

private:
  std::shared_ptr<WorkerPool> mWorkerPool;
  boost::optional<ElgamalPrivateKey> mPseudonymKey;
  std::shared_ptr<PseudonymTranslator> mPseudonymTranslator;
  std::shared_ptr<DataTranslator> mDataTranslator;
  std::shared_ptr<TranscryptorStorage> mStorage;
  std::shared_ptr<Metrics> lpMetrics;
  VerifiersResponse mVerifiers;
  uintmax_t mNextTranscryptorRequestNumber = 1U;
  uintmax_t mNextLogIssuedTicketRequestNumber = 1U;
};

}
