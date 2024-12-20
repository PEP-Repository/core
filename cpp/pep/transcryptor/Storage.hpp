#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/transcryptor/ChecksumChain.hpp>
#include <pep/utils/PropertyBasedContainer.hpp>

#include <filesystem>

namespace pep {

class TranscryptorStorage {
private:
  std::shared_ptr<TranscryptorStorageBackend> mStorage;
  std::string mPath;
  PropertyBasedContainer<std::unique_ptr<transcryptor::ChecksumChain>, &transcryptor::ChecksumChain::name>::set mChecksumChains;

  void ensureInitialized();
  void ensureInitialized_unguarded(bool& migrated);
  void migrate();
  void migrate_from_v1_to_v2();

  int64_t getOrCreatePseudonymSet(const std::vector<LocalPseudonym>& ps);
  int64_t getOrCreateColumnSet(std::vector<std::string> cols);
  int64_t getOrCreateModeSet(std::vector<std::string> modes);
  std::optional<int64_t> getOrCreateCertificateChain(
      X509CertificateChain&& chain);
  std::optional<uint64_t> getCurrentVersion();

 public:

  std::string getPath() const { return mPath; }

  TranscryptorStorage(const std::filesystem::path& path);

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
