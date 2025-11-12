#pragma once

#include <pep/async/WorkerPool.hpp>
#include <pep/rsk-pep/DataTranslator.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>

#include <pep/transcryptor/Storage.hpp>

#include <optional>

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
  protected:
    EnrolledParty enrollsAs() const noexcept override { return EnrolledParty::Transcryptor; }

  public:
    Parameters(
        std::shared_ptr<boost::asio::io_context> io_context,
        const Configuration& config);

    std::shared_ptr<PseudonymTranslator> getPseudonymTranslator() const;
    std::shared_ptr<DataTranslator> getDataTranslator() const;
    void setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator);
    void setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator);

    std::shared_ptr<TranscryptorStorage> getStorage() const;
    void setStorage(std::shared_ptr<TranscryptorStorage> storage);

    const VerifiersResponse& getVerifiers() const;
    void setVerifiers(const VerifiersResponse& verifiers);

    std::optional<ElgamalPrivateKey> getPseudonymKey() const;
    void setPseudonymKey(const ElgamalPrivateKey& key);

   protected:
    void check() const override;

   private:
    std::optional<ElgamalPrivateKey> pseudonymKey;
    std::shared_ptr<PseudonymTranslator> pseudonymTranslator;
    std::shared_ptr<DataTranslator> dataTranslator;
    std::shared_ptr<TranscryptorStorage> storage;
    std::optional<VerifiersResponse> verifiers;
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
  messaging::MessageBatches handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> pRequest);
  messaging::MessageBatches handleTranscryptorRequest(std::shared_ptr<TranscryptorRequest> pRequest, messaging::MessageSequence entriesObservable);
  messaging::MessageBatches handleRekeyRequest(std::shared_ptr<RekeyRequest> pRequest);
  messaging::MessageBatches handleLogIssuedTicketRequest(std::shared_ptr<LogIssuedTicketRequest> request);

  static std::string ComputePseudonymHash(
    const std::vector<LocalPseudonyms>& pseuds);

private:
  std::shared_ptr<WorkerPool> mWorkerPool;
  std::optional<ElgamalPrivateKey> mPseudonymKey;
  std::shared_ptr<PseudonymTranslator> mPseudonymTranslator;
  std::shared_ptr<DataTranslator> mDataTranslator;
  std::shared_ptr<TranscryptorStorage> mStorage;
  std::shared_ptr<Metrics> lpMetrics;
  VerifiersResponse mVerifiers;
  uintmax_t mNextTranscryptorRequestNumber = 1U;
  uintmax_t mNextLogIssuedTicketRequestNumber = 1U;
};

}
