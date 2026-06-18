#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include <pep/rsk/Proofs.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/transcryptor/ChecksumChain.hpp>
#include <pep/utils/PropertyBasedContainer.hpp>

#include <filesystem>

namespace pep {

class TranscryptorStorage {
private:
  std::shared_ptr<TranscryptorStorageBackend> mStorage;
  std::string path_;
  PropertyBasedContainer<std::unique_ptr<transcryptor::ChecksumChain>, &transcryptor::ChecksumChain::name>::set mChecksumChains;

  void ensureInitialized();
  void ensureInitialized_unguarded(bool& migrated);
  void migrate();
  void migrate_from_v1_to_v2();
  void removeOutdatedRecords();

  int64_t getOrCreatePseudonymSet(const std::vector<LocalPseudonym>& ps);
  int64_t getOrCreateColumnSet(std::vector<std::string> cols);
  int64_t getOrCreateModeSet(std::vector<std::string> modes);
  std::optional<int64_t> getOrCreateCertificateChain(
      X509CertificateChain chain);
  std::pair<std::string, std::optional<int64_t>> extractCertificateChain(SignedTicketRequest2 request);
  std::optional<uint64_t> getCurrentVersion();

 public:

  std::string getPath() const { return path_; }

  TranscryptorStorage(const std::filesystem::path& path);

  /// Retrieve stored verifiers for domain & session corresponding to certificate.
  std::optional<ReshuffleRekeyVerifiers> getUserVerifiers(const X509Certificate& userCertificate);
  /// Check consistency of verifiers with stored verifiers for domain (and session) corresponding to certificate.
  /// If consistent, stores the new verifiers.
  /// \throws std::runtime_error when inconsistent with stored verifiers.
  void checkAndStoreUserVerifiers(const X509Certificate& userCertificate, const ReshuffleRekeyVerifiers& verifiers);

  std::string logTicketRequest(
    const std::vector<LocalPseudonym>& localPseudonyms,
    const std::vector<std::string>& modes,
    SignedTicketRequest2 ticketRequest,
    std::string pseudonymHash);

  void logIssuedTicket(
    const std::string& id,
    const std::string& pseudonymHash,
    std::vector<std::string> columns,
    std::vector<std::string> modes,
    const std::string& accessGroup,
    Timestamp timestamp);

  std::vector<std::string> getChecksumChainNames();

  void computeChecksum(const std::string& chain, std::optional<uint64_t> maxCheckpoint,
        uint64_t& checksum, uint64_t& checkpoint);
};

}
