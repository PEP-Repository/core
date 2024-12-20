#pragma once

#include <pep/server/Server.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/utils/XxHasher.hpp>
#include <pep/storagefacility/FileStore.hpp>
#include <pep/storagefacility/StorageFacilityMessages.hpp>
#include <pep/storagefacility/SFId.hpp>

#include <filesystem>

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
    Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config);

    /*!
    * \return The pseudonym key
    */
    const ElgamalPrivateKey& getPseudonymKey() const;
    /*!
    * \param pseudonymKey The pseudonym key
    * \return The Parameters instance itself
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
    void check() override;

  private:
    boost::optional<ElgamalPrivateKey> pseudonymKey;
    boost::optional<std::string> encIdKey;
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
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleDataEnumerationRequest2(std::shared_ptr<SignedDataEnumerationRequest2> request);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleDataStoreRequest2(std::shared_ptr<SignedDataStoreRequest2> lpRequest, rxcpp::observable<std::shared_ptr<std::string>> mChunksObservable);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleMetadataStoreRequest2(std::shared_ptr<SignedMetadataUpdateRequest2> lpRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleMetadataReadRequest2(std::shared_ptr<SignedMetadataReadRequest2> lpRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleDataReadRequest2(std::shared_ptr<SignedDataReadRequest2> lpRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleDataDeleteRequest2(std::shared_ptr<SignedDataDeleteRequest2> lpRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
    handleDataHistoryRequest2(std::shared_ptr<SignedDataHistoryRequest2> lpRequest);

  std::string encryptId(const std::string& path, uint64_t time);
  SFId decryptId(const std::string& encId);
  std::vector<std::optional<LocalPseudonym>> decryptLocalPseudonyms(const std::vector<LocalPseudonyms>& source, std::vector<uint32_t> const *indices) const;

  Metadata compileMetadata(std::string column, const FileStore::Entry& entry);

  std::shared_ptr<EGCache> getCache();

  template <typename TEntry>
  using GetEntryContent = std::function<std::unique_ptr<EntryContent>(const TEntry&)>;
  typedef std::function<std::string(EpochMillis, const std::vector<std::string>&, XxHasher::Hash)> GetDataAlterationResponse;

  template <typename TRequest>
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleDataAlterationRequest(
    std::shared_ptr<Signed<TRequest>> signedRequest,
    rxcpp::observable<std::shared_ptr<std::string>> chunks,
    bool requireContentOverwrite,
    const GetEntryContent<typename TRequest::Entry> getEntryContent,
    const GetDataAlterationResponse& getResponse);

  void emitPage(rxcpp::subscriber<rxcpp::observable<std::shared_ptr<std::string>>> subscriber, uint32_t fileIndex, uint32_t pageIndex, rxcpp::observable<std::shared_ptr<std::string>> page) const;

private:
  ElgamalPrivateKey mPseudonymKey;
  std::string mEncIdKey;
  std::shared_ptr<WorkerPool> mWorkerPool;
  std::shared_ptr<FileStore> mFileStore;
  std::shared_ptr<Metrics> mMetrics;
  boost::asio::deadline_timer mTimer;
  const uint8_t mParallelisationWidth = 0; // passed to RxParallelConcat
  std::shared_ptr<EGCache> mCache;
};

}
