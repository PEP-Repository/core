#include <pep/storagefacility/StorageFacility.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/crypto/CPRNG.hpp>
#include <pep/utils/Hasher.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxParallelConcat.hpp>
#include <pep/utils/ApplicationMetrics.hpp>
#include <pep/auth/EnrolledParty.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/storagefacility/SFIdSerializer.hpp>
#include <pep/messaging/MessageHeader.hpp>
#include <pep/utils/Defer.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/placeholders.hpp>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-tap.hpp>

#include <unordered_map>
#include <sstream>

#include <prometheus/gauge.h>
#include <prometheus/summary.h>
#include <prometheus/counter.h>

using namespace std::chrono_literals;

namespace pep {

namespace {

constexpr size_t ENUMERATION_RESPONSE_MAX_ENTRIES = 2500;
constexpr size_t PAYLOAD_PAGES_MAX_CONCURRENCY = 1000; // Prevent excessive memory use: see https://gitlab.pep.cs.ru.nl/pep/ppp-config/-/issues/166#note_50515

class TicketIndices {
public:
  using Index = decltype(IndexList::mIndices)::value_type;

private:
  std::unordered_map<std::string, Index> mColumns;
  std::unordered_map<LocalPseudonym, Index> mPseudonyms;

public:
  TicketIndices(const Ticket2& ticket, const ElgamalPrivateKey& pseudonymKey) {
    if (ticket.mColumns.size() > std::numeric_limits<Index>::max()) {
      throw std::runtime_error("Ticket contains too many columns to map into an IndexList");
    }
    for (size_t i = 0U; i < ticket.mColumns.size(); ++i) {
      mColumns[ticket.mColumns[i]] = static_cast<Index>(i);
    }

    if (ticket.mPseudonyms.size() > std::numeric_limits<Index>::max()) {
      throw std::runtime_error("Ticket contains too many pseudonyms to map into an IndexList");
    }
    // TODO keep a decryption cache?  If a ticket with a lot of pseudonyms is
    // reused often (for each file), then we're wasting a lot of time.
    // See issue #592.
    for (size_t i = 0U; i < ticket.mPseudonyms.size(); ++i) {
      LocalPseudonym sfPseud = ticket.mPseudonyms[i].mStorageFacility.decrypt(pseudonymKey);
      mPseudonyms[sfPseud] = static_cast<Index>(i);
    }
  }

  Index getColumnIndex(const std::string& column) const {
    auto position = mColumns.find(column);
    if (position == mColumns.cend()) {
      throw Error("Ticket does not grant access to that column");
    }
    return position->second;
  }

  void verifyColumnAccess(const std::string& column) const {
    this->getColumnIndex(column); // Raises an Error if the ticket didn't contain the column
  }

  Index getPseudonymIndex(const LocalPseudonym& spPseud) {
    auto position = mPseudonyms.find(spPseud);
    if (position == mPseudonyms.cend()) {
      throw Error("Ticket does not grant access to that participant");
    }
    return position->second;
  }

  void verifyPseudonymAccess(const LocalPseudonym& spPseud) {
    this->getPseudonymIndex(spPseud); // Raises an Error if the ticket didn't contain the participant
  }
};

const std::string LOG_TAG("StorageFacility");

} // End anonymous namespace

StorageFacility::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  data_stored_bytes(prometheus::BuildCounter()
    .Name("pep_sf_stored_bytes")
    .Help("Total amount of bytes in datapages received by clients to be stored")
    .Register(*registry)
    .Add({})),
  data_retrieved_bytes(prometheus::BuildCounter()
    .Name("pep_sf_retrieved_bytes")
    .Help("Total amount of data in datapages sent to clients")
    .Register(*registry)
    .Add({})),
  dataRead_request_duration(prometheus::BuildSummary()
    .Name("pep_sf_dataRead_request_duration_seconds")
    .Help("Duration of a DataReadRequest2")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })),
  dataStore_request_duration(prometheus::BuildSummary()
    .Name("pep_sf_dataStore_request_duration_seconds")
    .Help("Duration of a DataStoreRequest2")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })),
  dataEnumeration_request_duration(prometheus::BuildSummary()
    .Name("pep_sf_dataEnumeration_request_duration_seconds")
    .Help("Duration of a DataEnumerationRequest2")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })),
  dataHistory_request_duration(prometheus::BuildSummary()
    .Name("pep_sf_dataHistory_request_duration_seconds")
    .Help("Duration of a DataHistoryRequest2")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })),
  entriesIncludingHistory(prometheus::BuildGauge()
    .Name("pep_sf_entries")
    .Help("Number of entries managed by FileStore, includes history of every file")
    .Register(*registry)
    .Add({})),
  entriesInMetaDir(prometheus::BuildGauge()
    .Name("pep_sf_meta_on_disk")
    .Help("Number of entries in the meta/ dir")
    .Register(*registry)
    .Add({}))
{ }

StorageFacility::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;
  std::filesystem::path encIdKeyFile;
  auto pageStoreConfig = std::make_shared<Configuration>();

  std::string strPseudonymKey;
  std::string encIdKey;

  try {
    auto pw = config.get<std::optional<uint8_t>>("ParallelisationWidth");
    if (pw.has_value()) {
      if (*pw == 0) {
        throw std::runtime_error("ParallelisationWidth cannot be 0.");
      }
      this->parallelisation_width = *pw;
      // For the default value,
      //   see the declaration of this->parallelisation_width.
    }

    encIdKeyFile = config.get<std::filesystem::path>("EncIdKeyFile");
    keysFile = std::filesystem::canonical(config.get<std::filesystem::path>("KeysFile"));
    this->storagePath = config.get<std::filesystem::path>("StoragePath");
    this->pageStoreConfig = std::make_shared<Configuration>(config.get_child("PageStore"));
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    Configuration keysConfig = Configuration::FromFile(keysFile);
    strPseudonymKey = boost::algorithm::unhex(keysConfig.get<std::string>("PseudonymKey"));
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with keys file: " << keysFile << " : " << e.what();
    throw;
  }

  // Why a seperate file for the EncIdKey?  Well, we want the key to be
  // auto-generated (if it doesn't exist yet) and it is likely that the
  // main keysfile is read-only.  (We wouldn't want to risk to overwrite it.)
  if (!std::filesystem::exists(encIdKeyFile)) {
    LOG(LOG_TAG, warning)
      << "The file " << encIdKeyFile << " does not exist. "
      << "Generating new one.  This should occur only once!";
    RandomBytes(encIdKey, 32);
    boost::property_tree::ptree root;
    root.add<std::string>(
      "Key",
      boost::algorithm::hex(encIdKey)
    );
    std::ofstream os(encIdKeyFile.string());
    std::filesystem::permissions(encIdKeyFile, std::filesystem::perms::owner_read);
    boost::property_tree::write_json(os, root);
  }
  else {
    Configuration encIdKeyConfig = Configuration::FromFile(encIdKeyFile);
    encIdKey = boost::algorithm::unhex(encIdKeyConfig.get<std::string>("Key"));
  }

  setPseudonymKey(CurveScalar(strPseudonymKey));
  setEncIdKey(encIdKey);
}

const ElgamalPrivateKey& StorageFacility::Parameters::getPseudonymKey() const {
  return pseudonymKey.value();
}

void StorageFacility::Parameters::setPseudonymKey(const ElgamalPrivateKey& pseudonymKey) {
  Parameters::pseudonymKey = pseudonymKey;
}

const std::string& StorageFacility::Parameters::getEncIdKey() const {
  return encIdKey.value();
}

void StorageFacility::Parameters::setEncIdKey(const std::string& key) {
  Parameters::encIdKey = key;
}

void StorageFacility::Parameters::check() const {
  if (!encIdKey)
    throw std::runtime_error("encIdKey must be set");
  if (!pseudonymKey)
    throw std::runtime_error("pseudonymKey must be set");
  SigningServer::Parameters::check();
  if (!this->pageStoreConfig)
    throw std::runtime_error("pageStoreConfig must be set");
  // FIXME: check if errors happend during startup of file store
}

std::vector<std::string> StorageFacility::getChecksumChainNames() const {
  return {
    "files",
    "entry-count"
  };
}

void StorageFacility::computeChecksumChainChecksum(
  const std::string& chain, std::optional<uint64_t> maxCheckpoint,
  uint64_t& checksum, uint64_t& checkpoint) {

  // both "files" and "entry-count" checksums are computed by adding
  // one entry at a time, via:
  std::function<void(const FileStore::EntryHeader&)> add;

  if (chain == "files") {
    add = [&checksum](const FileStore::EntryHeader& header) {
      auto checksumSubstitute = header.checksumSubstitute;
      checksum ^= checksumSubstitute;
    };
  }
  else if (chain == "entry-count") {
    add = [&checksum](const FileStore::EntryHeader& header) {
      checksum++;
    };
  }
  else {
    throw Error("Unknown checksumchain");
  }

  checksum = 0;
  checkpoint = 0;

  if (!maxCheckpoint) {
    // The Storage Facility uses a timestamp as checkpoint
    maxCheckpoint = TicksSinceEpoch<std::chrono::milliseconds>(TimeNow() - 1min);
  }

  mFileStore->forEachEntryHeader([add, &checkpoint, max = *maxCheckpoint](const FileStore::EntryHeader& header) {
    std::uint64_t validFromMs{static_cast<std::uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(header.validFrom))};
    if (validFromMs <= max) {
      checkpoint = std::max(checkpoint, validFromMs);
      add(header);
    }
    });
}

messaging::MessageBatches
StorageFacility::handleDataEnumerationRequest2(std::shared_ptr<SignedDataEnumerationRequest2> signedRequest) {
  LOG(LOG_TAG, debug) << "Received DataEnumerationRequest2";

  auto time = std::chrono::steady_clock::now();
  const auto& rootCAs = *this->getRootCAs();

  auto certified = signedRequest->certify(*this->getRootCAs());
  const auto& request = certified.message;
  auto accessGroup = certified.signatory.organizationalUnit();
  auto ticket = request.mTicket.open(rootCAs, accessGroup, "read-meta");

  struct ResponseEntry {
    DataEnumerationEntry2 entry;
    std::shared_ptr<FileStore::Entry> fileStoreEntry;
  };
  std::vector<ResponseEntry> responseEntries;

  // Look-up table to check whether to include column
  std::vector<std::string> includeColumn;
  if (request.mColumns) {
    includeColumn.reserve(request.mColumns->mIndices.size());
    for (uint32_t idx : request.mColumns->mIndices) {
      includeColumn.push_back(ticket.mColumns.at(idx));
    }
  }
  else {
    includeColumn.reserve(ticket.mColumns.size());
    for (const auto& column : ticket.mColumns) {
      includeColumn.push_back(column);
    }
  }

  // Create column-to-ticket-column-index look-up-table
  std::unordered_map<std::string, uint32_t> columnIndex;
  columnIndex.reserve(ticket.mColumns.size());
  for (uint32_t i = 0; i < ticket.mColumns.size(); i++) {
    columnIndex[ticket.mColumns[i]] = i;
  }
  // Decrypt pseudonyms.
  auto localPseudonyms = this->decryptLocalPseudonyms(ticket.mPseudonyms, request.mPseudonyms.has_value() ? &request.mPseudonyms->mIndices : nullptr);

  std::vector<uint64_t> ids; // used to lookup id from responseEntry mIndex
  for (size_t pseud_index = 0; pseud_index < localPseudonyms.size(); pseud_index++) {
    if (!localPseudonyms[pseud_index].has_value()) {
      continue;
    }
    for (const auto& col : includeColumn) {
      auto colIndexIt = columnIndex.find(col);
      if (colIndexIt == columnIndex.end()) {
        continue;
      }

      // enumerateData returns an error if there are no entries, which
      // we will ignore. Other errors are already logged.
      EntryName key(*localPseudonyms[pseud_index], col);
      auto entry = mFileStore->lookup(key, ticket.mTimestamp);
      if (!entry) {
        continue;
      }
      const auto& content = entry->content();
      if (content == nullptr) {
        continue;
      }
      assert(content->payload() != nullptr);

      auto& re = responseEntries.emplace_back();
      re.fileStoreEntry = entry;
      re.entry.mMetadata = this->compileMetadata(col, *entry);
      re.entry.mFileSize = entry->content()->payload()->size();
      re.entry.mPolymorphicKey = content->getPolymorphicKey(); // will be rerandomized later
      re.entry.mColumnIndex = colIndexIt->second;
      re.entry.mPseudonymIndex = static_cast<uint32_t>(pseud_index);
    }
  }

  if (responseEntries.size() > std::numeric_limits<uint32_t>::max()) {
    // Would overflow mIndex otherwise.
    throw Error("Number of matching entries exceeds uint32");
  }

  struct StreamContext {
    // Context used earlier
    CPRNG cprng;
    decltype(time) start_time;
  };
  auto ctx = std::make_shared<StreamContext>();
  ctx->start_time = time;

  // Rerandomize encrypted polymorphic keys and add the encrypted
  // SF identifiers.
  return mWorkerPool->batched_map<8>(std::move(responseEntries),
    observe_on_asio(*getIoContext()),
    [ctx, this](ResponseEntry re) {
      re.entry.mPolymorphicKey = this->getEgCache().rerandomize(
        re.entry.mPolymorphicKey,
        &ctx->cprng
      );
      re.entry.mPolymorphicKey.ensurePacked();

      auto& sfEntry = re.fileStoreEntry;
      re.entry.mId = encryptId(sfEntry->getName().string(), sfEntry->getValidFrom());

      return re;
    })
    .map([](std::vector<ResponseEntry> respEntries) {return std::make_shared<std::vector<ResponseEntry>>(std::move(respEntries)); }) // Ensure flat_map gets a cheaply copyable parameter value. See #1019
    .flat_map([ctx, server = SharedFrom(*this)](std::shared_ptr<std::vector<ResponseEntry>> respEntriesPtr)
      -> messaging::MessageBatches {

      // Generate response(s)
      std::vector<DataEnumerationResponse2> responseMsgs;
      responseMsgs.emplace_back();
      size_t i = 0;
      for (const auto& re : *respEntriesPtr) {
        responseMsgs.back().mEntries.push_back(re.entry);

        // We use mIndex to lookup the primary key in ids when serving data below.
        // The client should not learn mIndex, so we clear it.
        responseMsgs.back().mEntries.back().mIndex = 0;
        if (++i == ENUMERATION_RESPONSE_MAX_ENTRIES) {
          i = 0;
          responseMsgs.back().mHasMore = true;
          responseMsgs.emplace_back();
        }
      }
      std::vector<messaging::MessageSequence> response;
      response.reserve(responseMsgs.size());
      for (const auto& msg : responseMsgs) {
        response.push_back(rxcpp::observable<>::from(
          std::make_shared<std::string>(Serialization::ToString(msg))));
      }

      server->mMetrics->dataEnumeration_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx->start_time).count()); // in seconds
      return RxIterate(std::move(response));
      });
}

messaging::MessageBatches
StorageFacility::handleMetadataReadRequest2(std::shared_ptr<SignedMetadataReadRequest2> signedRequest) {
  return rxcpp::observable<>::just(CreateObservable<std::shared_ptr<std::string>>([signedRequest, server = SharedFrom(*this)](rxcpp::subscriber<std::shared_ptr<std::string>> subscriber) {
    auto rootCAs = server->getRootCAs();
    auto certified = signedRequest->certify(*rootCAs);
    const auto& request = certified.message;
    auto userGroup = certified.signatory.organizationalUnit();

    auto ticket = request.mTicket.open(
      *rootCAs,
      certified.signatory.organizationalUnit(),
      "read-meta"
    );

    // Create look-up-tables for columns and pseudonyms from ticket
    TicketIndices indices(ticket, server->mPseudonymKey);

    // Create initial response object
    auto response = std::make_shared<DataEnumerationResponse2>();
    // (Lambda that) sends the current response object to the subscriber and assigns a new, empty (followup) response object to the "response" variable
    auto sendResponse = [subscriber, &response]() {
      auto serialized = std::make_shared<std::string>(Serialization::ToString(*response));
      if (serialized->size() >= messaging::MAX_SIZE_OF_MESSAGE) {
        throw std::runtime_error("Enumeration response too large to send out");
      }
      subscriber.on_next(serialized);
      response = std::make_shared<DataEnumerationResponse2>();
    };

    CPRNG cprng;
    for (size_t i = 0; i < request.mIds.size(); i++) {
      // TODO execute decryption in WorkerPool
      auto sfid = server->decryptId(request.mIds[i]);
      auto sfentry = server->mFileStore->lookup(EntryName::Parse(sfid.mPath), sfid.mTime);
      if (sfentry == nullptr) {
        throw Error("openExistingDataEntry failed");
      }
      const auto& sfcontent = sfentry->content();
      if (sfcontent == nullptr) {
        throw Error("Cannot read data of a deleted entry");
      }
      assert(sfcontent->payload() != nullptr);

      // Parse entry name into properties
      LocalPseudonym pseud = sfentry->getName().pseudonym();
      std::string column = sfentry->getName().column();

      DataEnumerationEntry2 entry;
      entry.mMetadata = server->compileMetadata(column, *sfentry);
      // TODO execute rerandomization in WorkerPool
      entry.mPolymorphicKey = server->getEgCache().rerandomize(sfcontent->getPolymorphicKey(), &cprng);
      entry.mFileSize = sfcontent->payload()->size();
      entry.mId = request.mIds[i];
      entry.mIndex = static_cast<uint32_t>(i);
      entry.mColumnIndex = indices.getColumnIndex(column);
      entry.mPseudonymIndex = indices.getPseudonymIndex(pseud);
      response->mEntries.push_back(std::move(entry));

      // Prevent individual DataEnumerationResponse2 messages from becoming too large
      if (response->mEntries.size() >= ENUMERATION_RESPONSE_MAX_ENTRIES) {
        response->mHasMore = true;
        sendResponse();
      }
    }

    // Always send a final response with mHasMore = false. If zero entries were requested, this will be the only response we send.
    sendResponse();
    subscriber.on_completed();
  }));
}

messaging::MessageBatches
StorageFacility::handleDataReadRequest2(std::shared_ptr<SignedDataReadRequest2> signedRequest) {
  auto time = std::chrono::steady_clock::now();

  auto rootCAs = this->getRootCAs();
  auto certified = signedRequest->certify(*rootCAs);
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  auto ticket = request.mTicket.open(
    *rootCAs,
    userGroup,
    "read"
  );

  // Create look-up-tables for columns and pseudonyms from ticket
  TicketIndices indices(ticket, mPseudonymKey);
  std::vector<std::shared_ptr<FileStore::Entry>> entries;
  entries.resize(request.mIds.size());

  // open files
  for (size_t i = 0; i < request.mIds.size(); i++) {
    // TODO execute decryption in WorkerPool
    auto sfid = decryptId(request.mIds[i]);
    auto entry = mFileStore->lookup(EntryName::Parse(sfid.mPath), sfid.mTime);
    if (entry == nullptr) {
      throw Error("openExistingDataEntry failed");
    }
    if (entry->isTombstone()) {
      throw Error("Cannot read data of a deleted entry");
    }

    entries[i] = entry;

    // Check permission
    indices.verifyColumnAccess(entry->getName().column());
    indices.verifyPseudonymAccess(entry->getName().pseudonym());
  }

  class StreamContext : public std::enable_shared_from_this<StreamContext>, public SharedConstructor<StreamContext> {
    friend class SharedConstructor<StreamContext>;

  private:
    std::vector<std::shared_ptr<FileStore::Entry>> mEntries;
    std::shared_ptr<Metrics> mMetrics;
    std::chrono::steady_clock::time_point mStartTime;
    std::optional<rxcpp::subscriber<messaging::MessageSequence>> mSubscriber;
    uint32_t mFileIndex = 0U;
    uint32_t mPageIndex = 0U;

    StreamContext(std::vector<std::shared_ptr<FileStore::Entry>> entries, std::shared_ptr<Metrics> metrics, std::chrono::steady_clock::time_point startTime)
      : mEntries(std::move(entries)), mMetrics(std::move(metrics)), mStartTime(startTime) {
    }

    /// \brief Provides the next individual page (observable) to the subscriber
    /// \return TRUE if a page (observable) was emitted; FALSE if not (i.e. we're done emitting pages)
    /// \remark To properly provide all page(observable)s to the subscriber, keep invoking this method
    ///         until it returns FALSE. Subsequent invocations will then return FALSE without doing anything.
    bool emitNextPage() {
      if (mFileIndex >= mEntries.size()) {
        if (mSubscriber.has_value()) {
          // TODO: postpone duration measurement until all page _contents_ (i.e. inner observables) have been processed
          mMetrics->dataRead_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - mStartTime).count()); // in seconds
          mSubscriber->on_completed();
          mSubscriber.reset();
        }
        return false;
      }

      assert(mSubscriber.has_value());

      auto& sfentry = mEntries[mFileIndex];
      assert(sfentry->content() != nullptr);
      assert(sfentry->content()->payload() != nullptr);

      auto pageCount = sfentry->content()->payload()->pageCount();
      if (pageCount != 0U) {
        assert(mPageIndex < pageCount); // Implies that pageCount != 0U

        mSubscriber->on_next(sfentry->readPage(mPageIndex)
          .map([self = SharedFrom(*this), fileIndex = mFileIndex, pageIndex = mPageIndex](std::shared_ptr<std::string> contents) {
            // Schedule a followup page (observable) when caller is done processing this page's contents.
            // PEP_DEFER ensures that the outer observable keeps going even if we raise an exception (on the inner observable) from this lambda
            PEP_DEFER(self->emitNextPage());

            auto page = Serialization::FromString<DataPayloadPage>(*contents);
            page.mIndex = fileIndex;
            page.mPageNumber = pageIndex;

            auto returnedPage = std::make_shared<std::string>(
              Serialization::ToString(std::move(page)));
            if (returnedPage->size() >= messaging::MAX_SIZE_OF_MESSAGE) {
              throw std::runtime_error("Data payload page too large to send out");
            }
            self->mMetrics->data_retrieved_bytes.Increment(
              static_cast<double>(returnedPage->size()));
            return returnedPage;
            }));
      }

      ++mPageIndex;
      if (mPageIndex >= pageCount) {
        ++mFileIndex;
        mPageIndex = 0U;
      }
      return true;
    }

  public:
    void emitTo(rxcpp::subscriber<messaging::MessageSequence> subscriber) {
      assert(mFileIndex == 0U);
      assert(mPageIndex == 0U);
      assert(!mSubscriber.has_value());

      mSubscriber = subscriber;
      /* We queue a batch of pages to be sent out "immediately" (i.e. as soon as possible),
       * but we don't queue more than PAYLOAD_PAGES_MAX_CONCURRENCY at the same time.
       * If there are more pages than the initial batch, a new page is scheduled only when
       * (the contents of) a previous page have been fully processed.
       * This keeps the the number of pages being processed under (or at)
       * PAYLOAD_PAGES_MAX_CONCURRENCY at all times.
       */
      for (size_t i = 0; i < PAYLOAD_PAGES_MAX_CONCURRENCY; ++i) {
        if (!this->emitNextPage()) {
          break; // The number of pages to send out is less than PAYLOAD_PAGES_MAX_CONCURRENCY
        }
      }
    }
  };

  auto ctx = StreamContext::Create(std::move(entries), mMetrics, time);

  return CreateObservable<messaging::MessageSequence>(
    [ctx](rxcpp::subscriber<messaging::MessageSequence> subscriber) {
      ctx->emitTo(subscriber);
    }
  );
}

template <typename TRequest>
messaging::MessageBatches StorageFacility::handleDataAlterationRequest(
  std::shared_ptr<Signed<TRequest>> signedRequest,
  messaging::MessageSequence tail,
  bool requireContentOverwrite,
  const GetEntryContent<typename TRequest::Entry> getEntryContent,
  const GetDataAlterationResponse& getResponse) {
    auto time = std::chrono::steady_clock::now();
    const auto& rootCAs = this->getRootCAs();
    auto certified = signedRequest->certify(*rootCAs);
    auto request = MakeSharedCopy(std::move(certified.message));
    auto ticket = request->mTicket.open(*rootCAs, certified.signatory.organizationalUnit());

    if (!ticket.hasMode("write")) {
      throw Error("Ticket is missing \"write\" access mode");
    }

    struct StreamContext {
      std::vector<std::shared_ptr<FileStore::EntryChange>> entries;
      std::vector<std::shared_ptr<LocalPseudonym>> pseudonyms;
      std::vector<std::string> ids;
      std::vector<std::string> errors;
      std::vector<uint64_t> fileSizes;
      decltype(time) start_time;
    };
    auto ctx = std::make_shared<StreamContext>();

    ctx->entries.resize(request->mEntries.size(), nullptr);
    ctx->pseudonyms.resize(request->mEntries.size());
    ctx->ids.resize(request->mEntries.size());
    ctx->fileSizes.resize(request->mEntries.size());
    ctx->start_time = time;

    std::unordered_map<uint32_t, std::shared_ptr<LocalPseudonym>> pseudonymLut;
    for (size_t i = 0; i < request->mEntries.size(); i++) {
      const auto& entry = request->mEntries[i];

      // Decrypt local pseudonym
      if (pseudonymLut.count(entry.mPseudonymIndex) == 0) {
        pseudonymLut[entry.mPseudonymIndex] = MakeSharedCopy(
          ticket.mPseudonyms.at(entry.mPseudonymIndex)
          .mStorageFacility.decrypt(mPseudonymKey));
      }
      ctx->pseudonyms[i] = pseudonymLut[entry.mPseudonymIndex];

      auto& col = ticket.mColumns.at(entry.mColumnIndex);

      // Modify entry, only creating a new one if we don't require an overwrite
      auto entryChange = mFileStore->modifyEntry(EntryName(*ctx->pseudonyms[i], col), !requireContentOverwrite);
      if (entryChange == nullptr) {
        throw Error("Cannot find cell to update");
      }
      if (requireContentOverwrite && entryChange->isTombstone()) {
        throw Error("Cannot update cell that has been previously cleared/deleted");
      }

      for (size_t j = 0; j < i; ++j) { // TODO: improve performance: we don't want an inner loop making this O(n^2)
        if (ctx->entries[j]->getName() == entryChange->getName()) {
          LOG(LOG_TAG, error) << "Single request contained duplicate entry change for " + entryChange->getName().string();
          // Don't send our internal representation (local pseudonym contained in the entry's name) back to the client
          throw Error("Cannot store multiple values for column " + col + ". The duplicate entries are at indices " + std::to_string(i) + " and " + std::to_string(j));
        }
      }
      entryChange->setContent(getEntryContent(entry));

      ctx->entries[i] = std::move(entryChange);
    }

    auto server = SharedFrom(*this);
    auto hasher = std::make_shared<XxHasher>(0ULL);

    return CreateObservable<messaging::MessageSequence>(
      [ctx, server, request, tail, hasher, this, getResponse](
        rxcpp::subscriber<messaging::MessageSequence>
        subscriber) {
        tail.map(
          [server, subscriber, ctx, request]
          (std::shared_ptr<std::string> rawPage) // incoming page
          -> rxcpp::observable<std::string> {// md5 of page
            MessageMagic magic{};
            try {
              magic = GetMessageMagic(*rawPage);
            }
            catch (const SerializeException&) {
              throw Error("raw page size too small to contain magic");
            }

            if (magic != MessageMagician<DataPayloadPage>::GetMagic()) {
              std::ostringstream ss;
              ss << "Expected page, but got "
                << DescribeMessageMagic(*rawPage);
              LOG(LOG_TAG, warning) << ss.str();
              ctx->errors.push_back(ss.str());
              // An exception will be raised by the call (below) to Serialization::FromString<DataPayloadPage>
            }

            auto page = Serialization::FromString<DataPayloadPage>(*rawPage);

            // Note that .at() tests bounds.
            auto& sfentry = ctx->entries.at(page.mIndex);
            auto fs = uint64_t{page.mPayloadData.size()};
            ctx->fileSizes[page.mIndex] += fs;

            if (fs > 100000000) {
              throw Error("Incoming page is too large");
            }
            return sfentry->appendPage(rawPage, fs, page.mPageNumber).tap(
              [server, rawPage](const std::string& md5hash) {
                server->mMetrics->data_stored_bytes.Increment(static_cast<double>(rawPage->size()));
              });
          })
          .as_dynamic()
          // We can't use merge here because the md5hashes need to be added to
          // the hasher in the correct order, so we use concat
          .op(RxParallelConcat(mParallelisationWidth))
          .subscribe(
            [hasher](std::string md5hash) { // on next
              hasher->update(md5hash);
            },
            [subscriber, ctx](std::exception_ptr e) { // error handler
              for (auto& handle : ctx->entries) {
                std::move(*handle).cancel();
              }
              subscriber.on_error(e);
            },
            [this, server, subscriber, ctx, hasher, getResponse]() { // file close
              auto time = TimeNow(); // Make all entries available/valid at the same moment: see #1631
              for (size_t i = 0; i < ctx->entries.size(); i++) {
                auto& entry = ctx->entries[i];
                try {
                  auto id = encryptId(entry->getName().string(), time);
                  std::move(*entry).commit(time);
                  ctx->ids[i] = id;
                }
                catch (std::exception& e) {
                  std::move(*entry).cancel();
                  ctx->ids[i].clear();
                  std::ostringstream ss;
                  ss << "File " << i << " is not sane: " << e.what();
                  LOG(LOG_TAG, warning) << ss.str();
                  ctx->errors.push_back(ss.str());
                }
                server->mMetrics->entriesIncludingHistory.Set(static_cast<double>(server->mFileStore->entryCount()));
              }

              if (!ctx->errors.empty()) {
                auto description = boost::algorithm::join(ctx->errors, "; ");
                subscriber.on_error(std::make_exception_ptr(Error(description)));
                // TODO: don't invoke on_next and on_completed anymore
              }

              subscriber.on_next(rxcpp::observable<>::from(
                MakeSharedCopy(getResponse(time, ctx->ids, hasher->digest()))));
              server->mMetrics->dataStore_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx->start_time).count()); // in seconds
              subscriber.on_completed();
            });
      });
}

messaging::MessageBatches StorageFacility::handleDataStoreRequest2(
  std::shared_ptr<SignedDataStoreRequest2> signedRequest,
  messaging::MessageSequence tail) {
  auto getEntryContent = [filestore = mFileStore](const DataStoreEntry2& entry) {
    auto metadata = filestore->makeMetadataMap(entry.mMetadata.extra());
    return std::make_unique<EntryContent>(
        std::move(metadata),
        EntryContent::PayloadData(
          { .polymorphicKey = entry.mPolymorphicKey,
            .blindingTimestamp = entry.mMetadata.getBlindingTimestamp(),
            .scheme = entry.mMetadata.getEncryptionScheme()
          },
          nullptr));
  };

  auto getResponse = [](Timestamp /* ignored */, const std::vector<std::string>& ids, XxHasher::Hash hash) {
    DataStoreResponse2 resp;
    resp.mIds = ids;
    resp.mHash = hash;
    return Serialization::ToString(
      std::move(resp));
  };

  return this->handleDataAlterationRequest<DataStoreRequest2>(signedRequest, tail, false, getEntryContent, getResponse);
}

messaging::MessageBatches
StorageFacility::handleMetadataStoreRequest2(std::shared_ptr<SignedMetadataUpdateRequest2> lpRequest) {
  const auto& rootCAs = this->getRootCAs();
  auto certified = lpRequest->certify(*rootCAs);
  auto request = MakeSharedCopy(std::move(certified.message));
  auto userGroup = certified.signatory.organizationalUnit();

  auto ticket = request->mTicket.open(*rootCAs, certified.signatory.organizationalUnit());

  if (!ticket.hasMode("write-meta")) {
    throw Error("Ticket is missing write-meta access mode");
  }

  // Fill a vector with indices of pseudonyms that we want/need decrypted
  std::vector<uint32_t> pseudIndices;
  pseudIndices.reserve(request->mEntries.size());
  std::transform(request->mEntries.cbegin(), request->mEntries.cend(), std::back_inserter(pseudIndices), [](const DataStoreEntry2& entry) {return entry.mPseudonymIndex; });

  // Decrypt pseudonyms.
  auto localPseudonyms = this->decryptLocalPseudonyms(ticket.mPseudonyms, &pseudIndices);

  std::vector<std::shared_ptr<FileStore::EntryChange>> changes;
  for (const auto& entry : request->mEntries) {
    auto column = ticket.mColumns[entry.mColumnIndex];
    assert(localPseudonyms[entry.mPseudonymIndex].has_value());
    EntryName key(*localPseudonyms[entry.mPseudonymIndex], column);

    auto entryChange = mFileStore->modifyEntry(key, false);
    if (entryChange == nullptr) {
      throw Error("Cannot find cell to update metadata for");
    }
    if (entryChange->isTombstone()) {
      throw Error("Cannot update metadata for a deleted cell");
    }
    if (DataPayloadPage::EncryptionIncludesMetadata(EncryptionScheme(entryChange->content()->getEncryptionScheme()))) {
      throw Error("Metadata for this cell cannot be updated without re-uploading the payload data");
    }

    // Since our EntryChange modifies an existing Entry, the (cloned) content should already contain the original payload timestamp
    auto originalPayloadEntryTimestamp = entryChange->content()->getOriginalPayloadEntryTimestamp();
    assert(originalPayloadEntryTimestamp.has_value());
    // ... and the associated payload
    auto payload = entryChange->content()->payload();
    assert(payload != nullptr);

    // We'll create a new content (with the original payload but) with
    // - the specified (re-blinded and re-encrypted) polymorphic key.
    // - the specified blinding timestamp, which needs to match the (re-blinded and re-encrypted) polymorphic key
    //   because it's included in (blinding of the keys used for) page encryption.
    // - the encryption scheme that the client has used to re-blind and re-encrypt the polymorphic key.
    // - the specified metadata "x-"entries.
    auto content = std::make_unique<EntryContent>(
        mFileStore->makeMetadataMap(entry.mMetadata.extra()),
        EntryContent::PayloadData{
          {
            .polymorphicKey = entry.mPolymorphicKey,
            .blindingTimestamp = entry.mMetadata.getBlindingTimestamp(),
            .scheme = entry.mMetadata.getEncryptionScheme()
          },
          payload},
        *originalPayloadEntryTimestamp);

    entryChange->setContent(std::move(content));
    changes.push_back(entryChange);
  }

  auto time = TimeNow(); // Make all entries available/valid at the same moment: see #1631
  MetadataUpdateResponse2 response;
  for (const auto& change : changes) {
    auto id = this->encryptId(change->getName().string(), time);
    std::move(*change).commit(time);
    response.mIds.push_back(id);
  }

  return rxcpp::observable<>::just(rxcpp::observable<>::just(MakeSharedCopy(Serialization::ToString(response))).as_dynamic());
}

messaging::MessageBatches
StorageFacility::handleDataDeleteRequest2(std::shared_ptr<SignedDataDeleteRequest2> signedRequest) {
  auto getEntryContent = [](const DataRequestEntry2& entry) {
    // Return a nullptr to indicate that the entry is deleted (i.e. a tombstone)
    return std::unique_ptr<EntryContent>();
  };

  auto getResponse = [](Timestamp timestamp, const std::vector<std::string>& ids, XxHasher::Hash hash) {
    assert(hash == XxHasher(0ULL).digest());
    assert(ids.size() <= std::numeric_limits<uint32_t>::max());

    DataDeleteResponse2 resp{
      .mTimestamp = timestamp,
      .mEntries{}, // Filled below
    };

    resp.mEntries.mIndices.reserve(ids.size());
    for (size_t i = 0U; i < ids.size(); ++i) {
      if (!ids[i].empty()) {
        resp.mEntries.mIndices.emplace_back(static_cast<uint32_t>(i));
      }
    }

    return Serialization::ToString(std::move(resp));
  };

  auto tail = rxcpp::observable<>::empty<std::shared_ptr<std::string>>();
  return this->handleDataAlterationRequest<DataDeleteRequest2>(signedRequest, tail, true, getEntryContent, getResponse);
}

std::vector<std::optional<LocalPseudonym>> StorageFacility::decryptLocalPseudonyms(const std::vector<LocalPseudonyms>& source, std::vector<uint32_t> const *indices) const {
  std::vector<bool> includePseudonym(source.size(), indices == nullptr); // Include all pseudonyms (initialize elements to "true") if no indices have been specified

  if (indices != nullptr) {
    // The "indices" vector may contain duplicates, but we don't want to decrypt the corresponding local pseudonym multiple times.
    // We therefore use our separate vector (with elements initialized to "false") indicating _once_ for every source item (index) whether it should be decrypted.
    for (auto i : *indices) {
      includePseudonym[i] = true;
    }
  }

  // TODO execute in WorkerPool
  std::vector<std::optional<LocalPseudonym>> result;
  result.reserve(source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    if (includePseudonym[i]) { // Caller wants/needs this pseudonym: decrypt it
      result.emplace_back(source[i].mStorageFacility.decrypt(mPseudonymKey));
    }
    else { // Caller doesn't need this pseudonym: don't decrypt
      result.emplace_back(std::nullopt);
    }
  }

  assert(result.size() == source.size()); // Return value indices correspond with "source" parameter indices
  return result;
}

messaging::MessageBatches
StorageFacility::handleDataHistoryRequest2(std::shared_ptr<SignedDataHistoryRequest2> lpRequest) {
  // TODO: consolidate duplicate code with handleDataEnumerationRequest2
  LOG(LOG_TAG, debug) << "Received DataHistoryRequest2";

  auto start_time = std::chrono::steady_clock::now();
  const auto& rootCAs = this->getRootCAs();
  auto certified = lpRequest->certify(*rootCAs);
  const auto& request = certified.message;

  auto accessGroup = certified.signatory.organizationalUnit();
  UserGroup::EnsureAccess({UserGroup::DataAdministrator, UserGroup::Watchdog}, accessGroup);

  auto ticket = request.mTicket.open(*rootCAs, accessGroup, "read-meta");

  DataHistoryResponse2 response;

  // Look-up table to check whether to include column
  std::vector<std::string> includeColumn;
  if (request.mColumns) {
    includeColumn.reserve(request.mColumns->mIndices.size());
    for (uint32_t idx : request.mColumns->mIndices)
      includeColumn.push_back(ticket.mColumns.at(idx));
  }
  else {
    includeColumn.reserve(ticket.mColumns.size());
    for (const auto& column : ticket.mColumns)
      includeColumn.push_back(column);
  }

  // Create column-to-ticket-column-index look-up-table
  std::unordered_map<std::string, uint32_t> columnIndex;
  columnIndex.reserve(ticket.mColumns.size());
  for (uint32_t i = 0; i < ticket.mColumns.size(); i++) {
    columnIndex[ticket.mColumns[i]] = i;
  }
  // Decrypt pseudonyms.
  auto localPseudonyms = this->decryptLocalPseudonyms(ticket.mPseudonyms, request.mPseudonyms.has_value() ? &request.mPseudonyms->mIndices : nullptr);

  std::vector<uint64_t> ids; // used to lookup id from responseEntry mIndex
  for (size_t pseud_index = 0; pseud_index < localPseudonyms.size(); pseud_index++) {
    if (!localPseudonyms[pseud_index].has_value()) {
      continue;
    }
    for (const auto& col : includeColumn) {
      auto colIndexIt = columnIndex.find(col);
      if (colIndexIt == columnIndex.end()) {
        continue;
      }

      const auto& localPseudonym = *localPseudonyms[pseud_index];
      // enumerateData returns an error if there are no entries, which
      // we will ignore. Other errors are already logged.
      EntryName key(localPseudonym, col);
      auto history = mFileStore->lookupWithHistory(key);
      for (const auto& entry : history) {
        assert(entry != nullptr);

        Timestamp validFrom = entry->getValidFrom();
        response.mEntries.push_back({
          .mColumnIndex = colIndexIt->second,
          .mPseudonymIndex = static_cast<uint32_t>(pseud_index),
          .mTimestamp = validFrom,
          .mId = !entry->isTombstone() ? encryptId(entry->getName().string(), validFrom) : "",
        });
      }
    }
  }

  mMetrics->dataHistory_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); // in seconds

  return rxcpp::observable<>::just(
    rxcpp::observable<>::just(MakeSharedCopy(Serialization::ToString(response))).as_dynamic());
}

std::string StorageFacility::encryptId(std::string path, Timestamp time) {
  return Serialization::ToString(
    EncryptedSFId(
      mEncIdKey,
      SFId{std::move(path), time}),
    false);
}

SFId StorageFacility::decryptId(std::string_view encId) {
  return Serialization::FromString<EncryptedSFId>(encId, false).decrypt(mEncIdKey);
}

Metadata StorageFacility::compileMetadata(
  std::string column,
  const FileStore::Entry& entry) {

  const auto& content = entry.content();
  assert(content != nullptr);

  Metadata result = Metadata(
    std::move(column),
    content->getBlindingTimestamp(),
    content->getEncryptionScheme());

  auto payloadTimestamp = content->getOriginalPayloadEntryTimestamp();
  if (payloadTimestamp.has_value()) {
    result.setOriginalPayloadEntryId(this->encryptId(entry.getName().string(), *payloadTimestamp));
  }

  // Extract the 'extra' PEP metadata entries from
  //  the file store entry's metadata - those entries that start with 'x-'.
  result.extra() = mFileStore->extractMetadataMap(content->metadata());

  return result;
}

std::optional<std::filesystem::path> StorageFacility::getStoragePath() {
  return EnsureDirectoryPath(mFileStore->metaDir());
}

void StorageFacility::statsTimer(const boost::system::error_code& e) {
  if (e == boost::asio::error::operation_aborted) {
    return;
  }
  auto metaDirs = std::filesystem::directory_iterator(mFileStore->metaDir());
  auto metaDirsCount = std::distance(metaDirs, std::filesystem::directory_iterator());

  mMetrics->entriesInMetaDir.Set(static_cast<double>(metaDirsCount));

  mTimer.expires_after(60s);
  mTimer.async_wait(boost::bind(&pep::StorageFacility::statsTimer, this, boost::asio::placeholders::error));
}

StorageFacility::StorageFacility(std::shared_ptr<pep::StorageFacility::Parameters> parameters)
  : SigningServer(parameters),
  mPseudonymKey(parameters->getPseudonymKey()), mEncIdKey(parameters->getEncIdKey()),
  mWorkerPool(WorkerPool::getShared()),
  mFileStore(FileStore::Create(
    parameters->getStoragePath().string(),
    parameters->getPageStoreConfig(),
    parameters->getIoContext(),
    mRegistry)),
  mMetrics(std::make_shared<Metrics>(mRegistry)),
  mTimer(*parameters->getIoContext()),
  mParallelisationWidth(parameters->getParallelisationWidth()) {
  RegisterRequestHandlers(*this,
                          &StorageFacility::handleMetadataReadRequest2,
                          &StorageFacility::handleDataReadRequest2,
                          &StorageFacility::handleDataStoreRequest2,
                          &StorageFacility::handleDataDeleteRequest2,
                          &StorageFacility::handleMetadataStoreRequest2,
                          &StorageFacility::handleDataEnumerationRequest2,
                          &StorageFacility::handleDataHistoryRequest2);

  mMetrics->entriesIncludingHistory.Set(static_cast<double>(mFileStore->entryCount()));
  statsTimer({});
}

}
