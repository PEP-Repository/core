#pragma once

#include <pep/server/SigningServer.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/utils/XxHasher.hpp>
#include <pep/storagefacility/FileStore.hpp>
#include <pep/storagefacility/StorageFacilityMessages.hpp>
#include <pep/storagefacility/SFId.hpp>

#include <boost/asio/steady_timer.hpp>
#include <filesystem>
#include <optional>

namespace pep {

class StorageFacility : public SigningServer {
private:
  class Metrics : public RegisteredMetrics {
  public:
    Metrics(std::shared_ptr<prometheus::Registry> registry);
    prometheus::Counter& data_stored_bytes;
    prometheus::Counter& data_retrieved_bytes;

    prometheus::Summary& dataRead_request_duration;
    prometheus::Summary& dataStore_request_duration;
    prometheus::Summary& dataEnumeration_request_duration;
    prometheus::Summary& dataHistory_request_duration;

    prometheus::Gauge& entriesIncludingHistory;
    prometheus::Gauge& entriesInMetaDir;
  };

public:
  class Parameters : public SigningServer::Parameters {
    friend class StorageFacility;
  public:
    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

    /*!
    * \return The pseudonym key
    */
    const ElgamalPrivateKey& getPseudonymKey() const;
    /*!
    * \param pseudonymKey The pseudonym key
    */
    void setPseudonymKey(const ElgamalPrivateKey& pseudonymKey);

    const std::string& getEncIdKey() const;
    void setEncIdKey(const std::string& key);

    uint8_t getParallelisationWidth() const {
      return this->parallelisation_width;
    }
    std::filesystem::path getStoragePath() const {
      return this->storagePath;
    }
    std::shared_ptr<Configuration> getPageStoreConfig() const {
      return this->pageStoreConfig;
    }

  protected:
    void check() const override;

  private:
    std::optional<ElgamalPrivateKey> pseudonymKey;
    std::optional<std::string> encIdKey;
    uint8_t parallelisation_width = 10; // passed to RxParalellConcat

    // passed to FileStore::Create
    std::filesystem::path storagePath;
    std::shared_ptr<Configuration> pageStoreConfig;
  };

public:
  explicit StorageFacility(std::shared_ptr<Parameters> parameters);

protected:
  std::string describe() const override;
  std::optional<std::filesystem::path> getStoragePath() override;
  void statsTimer(const boost::system::error_code& e);
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain,
    std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
    uint64_t& checkpoint) override;

private:
  messaging::MessageBatches handleDataEnumerationRequest2(std::shared_ptr<SignedDataEnumerationRequest2> request);
  messaging::MessageBatches handleDataStoreRequest2(std::shared_ptr<SignedDataStoreRequest2> lpRequest, messaging::MessageSequence tail);
  messaging::MessageBatches handleMetadataStoreRequest2(std::shared_ptr<SignedMetadataUpdateRequest2> lpRequest);
  messaging::MessageBatches handleMetadataReadRequest2(std::shared_ptr<SignedMetadataReadRequest2> lpRequest);
  messaging::MessageBatches handleDataReadRequest2(std::shared_ptr<SignedDataReadRequest2> lpRequest);
  messaging::MessageBatches handleDataDeleteRequest2(std::shared_ptr<SignedDataDeleteRequest2> lpRequest);
  messaging::MessageBatches handleDataHistoryRequest2(std::shared_ptr<SignedDataHistoryRequest2> lpRequest);

  std::string encryptId(std::string path, Timestamp time);
  SFId decryptId(std::string_view encId);
  std::vector<std::optional<LocalPseudonym>> decryptLocalPseudonyms(const std::vector<LocalPseudonyms>& source, std::vector<uint32_t> const *indices) const;

  Metadata compileMetadata(std::string column, const FileStore::Entry& entry);

  template <typename TEntry>
  using GetEntryContent = std::function<std::unique_ptr<EntryContent>(const TEntry&)>;
  using GetDataAlterationResponse = std::function<std::string(Timestamp, const std::vector<std::string>&, XxHasher::Hash)>;

  template <typename TRequest>
  messaging::MessageBatches handleDataAlterationRequest(
    std::shared_ptr<Signed<TRequest>> signedRequest,
    messaging::MessageSequence tail,
    bool requireContentOverwrite,
    const GetEntryContent<typename TRequest::Entry> getEntryContent,
    const GetDataAlterationResponse& getResponse);

private:
  ElgamalPrivateKey mPseudonymKey;
  std::string mEncIdKey;
  std::shared_ptr<WorkerPool> mWorkerPool;
  std::shared_ptr<FileStore> mFileStore;
  std::shared_ptr<Metrics> mMetrics;
  boost::asio::steady_timer mTimer;
  const uint8_t mParallelisationWidth = 0; // passed to RxParallelConcat
};

}
