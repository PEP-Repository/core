#pragma once

#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/auth/ServerTraits.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp> // for ElgamalPrivateKey
#include <pep/key-components/KeyComponentServer.hpp>
#include <pep/messaging/MessageSequence.hpp>
#include <pep/metrics/RegisteredMetrics.hpp>
#include <pep/morphing/ServerVerifiers.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/server/ChecksumChain.hpp>
#include <pep/ticketing/TicketingMessages.hpp> // for LocalPseudonyms
#include <pep/transcryptor/Storage.hpp>
#include <pep/transcryptor/TranscryptorMessages.hpp>
#include <pep/utils/Configuration_fwd.hpp>

#include <prometheus/registry.h>

#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace pep {

class Transcryptor : public KeyComponentServer {
 private:
  class Metrics : public RegisteredMetrics {
   public:
    Metrics(std::shared_ptr<prometheus::Registry> registry);
    prometheus::Summary& transcryptorRequestDuration;
    prometheus::Gauge& transcryptorLogSize;
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

    const EndPoint& getAccessManagerEndPoint() const { return accessManagerEndPoint_; }

   protected:
    void check() const override;

   private:
    std::optional<ElgamalPrivateKey> pseudonymKey_;
    std::shared_ptr<TranscryptorStorage> storage_;
    std::optional<ServerVerifiers> verifiers_;

    EndPoint accessManagerEndPoint_;
  };

public:
  explicit Transcryptor(std::shared_ptr<Parameters> parameters);

protected:
  std::optional<std::filesystem::path> getStoragePath() override;
  std::shared_ptr<prometheus::Registry> getMetricsRegistry() override;
  const ChecksumChains& checksumChains() const noexcept override;

private:
  messaging::MessageBatches handleTranscryptorRequest(std::shared_ptr<TranscryptorRequest> pRequest, messaging::MessageSequence entriesObservable);
  messaging::MessageBatches handleRekeyRequest(std::shared_ptr<RekeyRequest> pRequest);
  messaging::MessageBatches handleLogIssuedTicketRequest(std::shared_ptr<LogIssuedTicketRequest> request);

private:
  std::shared_ptr<WorkerPool> workerPool_;
  std::optional<ElgamalPrivateKey> pseudonymKey_;
  AccessManagerProxy accessManagerProxy_;
  std::shared_ptr<TranscryptorStorage> storage_;
  std::shared_ptr<Metrics> lpMetrics_;
  ServerVerifiers verifiers_;
  uintmax_t nextTranscryptorRequestNumber_ = 1U;
  uintmax_t nextLogIssuedTicketRequestNumber_ = 1U;
};

/// Hashes a pair of pseudonym lists into a single value.
///
/// Result depends on both the order of the lists and (the order of) their elements.
std::string ComputePseudonymHash(std::span<const LocalPseudonyms>, std::span<const LocalPseudonyms>);
}
