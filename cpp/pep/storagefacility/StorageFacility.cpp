#include <pep/storagefacility/StorageFacility.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Hasher.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxParallelConcat.hpp>
#include <pep/utils/ApplicationMetrics.hpp>
#include <pep/auth/EnrolledParty.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/storagefacility/SFIdSerializer.hpp>
#include <pep/messaging/MessageHeader.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/morphing/MorphingPropertySerializers.hpp>

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
  using Index = decltype(IndexList::indices_)::value_type;

private:
  std::unordered_map<std::string, Index> columns_;
  std::unordered_map<LocalPseudonym, Index> pseudonyms_;

public:
  TicketIndices(const Ticket2& ticket, const ElgamalPrivateKey& pseudonymKey) {
    if (ticket.columns.size() > std::numeric_limits<Index>::max()) {
      throw std::runtime_error("Ticket contains too many columns to map into an IndexList");
    }
    for (size_t i = 0U; i < ticket.columns.size(); ++i) {
      columns_[ticket.columns[i]] = static_cast<Index>(i);
    }

    if (ticket.accessSubjects.size() > std::numeric_limits<Index>::max()) {
      throw std::runtime_error("Ticket contains too many subjects to map into an IndexList");
    }
    // TODO keep a decryption cache?  If a ticket with a lot of pseudonyms is
    // reused often (for each file), then we're wasting a lot of time.
    // See issue #592.
    for (size_t i = 0U; i < ticket.accessSubjects.size(); ++i) {
      LocalPseudonym sfPseud = ticket.accessSubjects[i].storageFacility.decrypt(pseudonymKey);
      pseudonyms_[sfPseud] = static_cast<Index>(i);
    }
  }

  Index getColumnIndex(const std::string& column) const {
    auto position = columns_.find(column);
    if (position == columns_.cend()) {
      throw Error("Ticket does not grant access to that column");
    }
    return position->second;
  }

  void verifyColumnAccess(const std::string& column) const {
    this->getColumnIndex(column); // Raises an Error if the ticket didn't contain the column
  }

  Index getPseudonymIndex(const LocalPseudonym& spPseud) {
    auto position = pseudonyms_.find(spPseud);
    if (position == pseudonyms_.cend()) {
      throw Error("Ticket does not grant access to that participant");
    }
    return position->second;
  }

  void verifyPseudonymAccess(const LocalPseudonym& spPseud) {
    this->getPseudonymIndex(spPseud); // Raises an Error if the ticket didn't contain the participant
  }
};

template <typename T>
bool ReadOptionalNonZeroConfigValue(T& destination, const Configuration& config, const std::string& key) {
  if (auto value = config.get<std::optional<T>>(key)) {
    if (*value == T(0)) {
      throw std::runtime_error(key + " cannot be 0");
    }
    destination = *value;
    return true;
  }

  return false;
}

const std::string LogTag("StorageFacility");

} // End anonymous namespace

StorageFacility::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  data_stored_bytes(prometheus::BuildCounter()
    .Name("pep_sf_stored_bytes")
    .Help("Total amount of bytes in datapages received by clients to be stored") // by this process (PID), i.e. during this session
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
  entriesIncludingHistory(prometheus::BuildGauge() // Defined as a gauge instead of a Counter (despite only increasing) so that we can .Set it
    .Name("pep_sf_entries")
    .Help("Number of entries managed by FileStore, includes history of every file")
    .Register(*registry)
    .Add({})),
  entriesInMetaDir(prometheus::BuildGauge()
    .Name("pep_sf_meta_on_disk")
    .Help("Number of entries in the meta/ dir") // normally the number of subdirectories, i.e. the number of rows ("participants")
    .Register(*registry)
    .Add({})),
  totalPayloadBytes(prometheus::BuildGauge() // Defined as a gauge instead of a Counter (despite only increasing) so that we can .Set it
    .Name("pep_total_payload_bytes")
    .Help("Total bytes in payload(page)s of all entries including history, rounded to configured resolution")
    .Register(*registry)
    .Add({})),
  rollingPayloadBytes(prometheus::BuildGauge()
    .Name("pep_rolling_payload_bytes")
    .Help("Total bytes in payload(page)s of current/latest/rolling data, rounded to configured resolution")
    .Register(*registry)
    .Add({}))
{ }

StorageFacility::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;
  std::filesystem::path encIdKeyFile;
  auto pageStoreConfig = std::make_shared<Configuration>();

  try {
    keysFile = config.get<std::filesystem::path>("EnrolledPartyKeysFile");

    // See the declaration/definition of the fields for default values
    ReadOptionalNonZeroConfigValue(parallelisationWidth_, config, "ParallelisationWidth");
    ReadOptionalNonZeroConfigValue(dataSizeResolution_, config, "DataSizeResolution");

    encIdKeyFile = config.get<std::filesystem::path>("EncIdKeyFile");
    storagePath_ = config.get<std::filesystem::path>("StoragePath");
    pageStoreConfig_ = std::make_shared<Configuration>(config.get_child("PageStore"));
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    auto enrolledPartyKeys = Configuration::FromFile(keysFile).get<EnrolledPartyKeys>("");
    setPseudonymKey(enrolledPartyKeys.pseudonymKey.value());
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with keys file: " << keysFile << " : " << e.what();
    throw;
  }

  // Why a seperate file for the EncIdKey?  Well, we want the key to be
  // auto-generated (if it doesn't exist yet) and it is likely that the
  // main keysfile is read-only.  (We wouldn't want to risk to overwrite it.)
  std::string encIdKey;
  if (!std::filesystem::exists(encIdKeyFile)) {
    PEP_LOG(LogTag, Severity::Warning)
      << "The file " << encIdKeyFile << " does not exist. "
      << "Generating new one.  This should occur only once!";
    encIdKey = RandomString(32);
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

  setEncIdKey(encIdKey);
}

const ElgamalPrivateKey& StorageFacility::Parameters::getPseudonymKey() const {
  return pseudonymKey_.value();
}

void StorageFacility::Parameters::setPseudonymKey(const ElgamalPrivateKey& pseudonymKey) {
  pseudonymKey_ = pseudonymKey;
}

const std::string& StorageFacility::Parameters::getEncIdKey() const {
  return encIdKey_.value();
}

void StorageFacility::Parameters::setEncIdKey(const std::string& key) {
  encIdKey_ = key;
}

void StorageFacility::Parameters::check() const {
  if (!encIdKey_)
    throw std::runtime_error("encIdKey must be set");
  if (!pseudonymKey_)
    throw std::runtime_error("pseudonymKey must be set");
  SigningServer::Parameters::check();
  if (!pageStoreConfig_)
    throw std::runtime_error("pageStoreConfig must be set");
  if (dataSizeResolution_ == 0U) {
    throw std::runtime_error("dataSizeResolution cannot be zero");
  }
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

  fileStore_->forEachEntryHeader([add, &checkpoint, max = *maxCheckpoint](const FileStore::EntryHeader& header) {
    std::uint64_t validFromMs{static_cast<std::uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(header.validFrom))};
    if (validFromMs <= max) {
      checkpoint = std::max(checkpoint, validFromMs);
      add(header);
    }
    });
}

messaging::MessageBatches
StorageFacility::handleDataEnumerationRequest2(std::shared_ptr<SignedDataEnumerationRequest2> signedRequest) {
  PEP_LOG(LogTag, Severity::Debug) << "Received DataEnumerationRequest2";

  auto time = std::chrono::steady_clock::now();
  const auto& rootCAs = *this->getRootCAs();

  auto certified = signedRequest->open(rootCAs);
  const auto& request = certified.message;
  auto accessGroup = certified.signatory.organizationalUnit();
  auto ticket = request.ticket_.open(rootCAs, accessGroup, "read-meta");

  struct ResponseEntry {
    DataEnumerationEntry2 entry;
    std::shared_ptr<FileStore::Entry> fileStoreEntry;
  };
  std::vector<ResponseEntry> responseEntries;

  // Look-up table to check whether to include column
  std::vector<std::string> includeColumn;
  if (request.columns_) {
    includeColumn.reserve(request.columns_->indices_.size());
    for (uint32_t idx : request.columns_->indices_) {
      includeColumn.push_back(ticket.columns.at(idx));
    }
  }
  else {
    includeColumn.reserve(ticket.columns.size());
    for (const auto& column : ticket.columns) {
      includeColumn.push_back(column);
    }
  }

  // Create column-to-ticket-column-index look-up-table
  std::unordered_map<std::string, uint32_t> columnIndex;
  columnIndex.reserve(ticket.columns.size());
  for (uint32_t i = 0; i < ticket.columns.size(); i++) {
    columnIndex[ticket.columns[i]] = i;
  }
  // Decrypt pseudonyms.
  auto localPseudonyms = this->decryptLocalPseudonyms(ticket.accessSubjects, request.pseudonyms_.has_value() ? &request.pseudonyms_->indices_ : nullptr);

  std::vector<uint64_t> ids; // used to lookup id from responseEntry index_
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
      auto entry = fileStore_->lookup(key, ticket.timestamp);
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
      re.entry.metadata_ = this->compileMetadata(col, *entry);
      re.entry.fileSize_ = entry->content()->payload()->size();
      re.entry.polymorphicKey_ = content->getPolymorphicKey(); // will be rerandomized later
      re.entry.columnIndex_ = colIndexIt->second;
      re.entry.pseudonymIndex_ = static_cast<uint32_t>(pseud_index);
    }
  }

  if (responseEntries.size() > std::numeric_limits<uint32_t>::max()) {
    // Would overflow index_ otherwise.
    throw Error("Number of matching entries exceeds uint32");
  }

  struct StreamContext {
    // Context used earlier
    decltype(time) startTime;
  };
  auto ctx = std::make_shared<StreamContext>();
  ctx->startTime = time;

  // Rerandomize encrypted polymorphic keys and add the encrypted
  // SF identifiers.
  return workerPool_->batched_map<8>(std::move(responseEntries),
    ObserveOnAsio(*getIoContext()),
    [ctx, this](ResponseEntry re) {
      re.entry.polymorphicKey_ = this->getEgCache().rerandomize(
        re.entry.polymorphicKey_
      );
      re.entry.polymorphicKey_.ensurePacked();

      auto& sfEntry = re.fileStoreEntry;
      re.entry.id_ = encryptId(sfEntry->getName().string(), sfEntry->getValidFrom());

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
        responseMsgs.back().entries_.push_back(re.entry);

        // We use index_ to lookup the primary key in ids when serving data below.
        // The client should not learn index_, so we clear it.
        responseMsgs.back().entries_.back().index_ = 0;
        if (++i == ENUMERATION_RESPONSE_MAX_ENTRIES) {
          i = 0;
          responseMsgs.back().hasMore_ = true;
          responseMsgs.emplace_back();
        }
      }
      std::vector<messaging::MessageSequence> response;
      response.reserve(responseMsgs.size());
      for (const auto& msg : responseMsgs) {
        response.push_back(rxcpp::observable<>::from(
          std::make_shared<std::string>(Serialization::ToString(msg))));
      }

      server->metrics_->dataEnumeration_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx->startTime).count()); // in seconds
      return RxIterate(std::move(response));
      });
}

messaging::MessageBatches
StorageFacility::handleMetadataReadRequest2(std::shared_ptr<SignedMetadataReadRequest2> signedRequest) {
  return rxcpp::observable<>::just(CreateObservable<std::shared_ptr<std::string>>([signedRequest, server = SharedFrom(*this)](rxcpp::subscriber<std::shared_ptr<std::string>> subscriber) {
    auto rootCAs = server->getRootCAs();
    auto certified = signedRequest->open(*rootCAs);
    const auto& request = certified.message;
    auto userGroup = certified.signatory.organizationalUnit();

    auto ticket = request.ticket_.open(
      *rootCAs,
      userGroup,
      "read-meta"
    );

    // Create look-up-tables for columns and pseudonyms from ticket
    TicketIndices indices(ticket, server->pseudonymKey_);

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

    for (size_t i = 0; i < request.ids_.size(); i++) {
      // TODO execute decryption in WorkerPool
      auto sfid = server->decryptId(request.ids_[i]);
      auto sfentry = server->fileStore_->lookup(EntryName::Parse(sfid.path), sfid.time);
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
      entry.metadata_ = server->compileMetadata(column, *sfentry);
      // TODO execute rerandomization in WorkerPool
      entry.polymorphicKey_ = server->getEgCache().rerandomize(sfcontent->getPolymorphicKey());
      entry.fileSize_ = sfcontent->payload()->size();
      entry.id_ = request.ids_[i];
      entry.index_ = static_cast<uint32_t>(i);
      entry.columnIndex_ = indices.getColumnIndex(column);
      entry.pseudonymIndex_ = indices.getPseudonymIndex(pseud);
      response->entries_.push_back(std::move(entry));

      // Prevent individual DataEnumerationResponse2 messages from becoming too large
      if (response->entries_.size() >= ENUMERATION_RESPONSE_MAX_ENTRIES) {
        response->hasMore_ = true;
        sendResponse();
      }
    }

    // Always send a final response with hasMore_ = false. If zero entries were requested, this will be the only response we send.
    sendResponse();
    subscriber.on_completed();
  }));
}

messaging::MessageBatches
StorageFacility::handleDataReadRequest2(std::shared_ptr<SignedDataReadRequest2> signedRequest) {
  auto time = std::chrono::steady_clock::now();

  auto rootCAs = this->getRootCAs();
  auto certified = signedRequest->open(*rootCAs);
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  auto ticket = request.ticket_.open(
    *rootCAs,
    userGroup,
    "read"
  );

  // Create look-up-tables for columns and pseudonyms from ticket
  TicketIndices indices(ticket, pseudonymKey_);
  std::vector<std::shared_ptr<FileStore::Entry>> entries;
  entries.resize(request.ids_.size());

  // open files
  for (size_t i = 0; i < request.ids_.size(); i++) {
    // TODO execute decryption in WorkerPool
    auto sfid = decryptId(request.ids_[i]);
    auto entry = fileStore_->lookup(EntryName::Parse(sfid.path), sfid.time);
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
    std::vector<std::shared_ptr<FileStore::Entry>> entries_;
    std::shared_ptr<Metrics> metrics_;
    std::chrono::steady_clock::time_point startTime_;
    std::optional<rxcpp::subscriber<messaging::MessageSequence>> subscriber_;
    uint32_t fileIndex_ = 0U;
    uint32_t pageIndex_ = 0U;

    StreamContext(std::vector<std::shared_ptr<FileStore::Entry>> entries, std::shared_ptr<Metrics> metrics, std::chrono::steady_clock::time_point startTime)
      : entries_(std::move(entries)), metrics_(std::move(metrics)), startTime_(startTime) {
    }

    /// \brief Provides the next individual page (observable) to the subscriber
    /// \return TRUE if a page (observable) was emitted; FALSE if not (i.e. we're done emitting pages)
    /// \remark To properly provide all page(observable)s to the subscriber, keep invoking this method
    ///         until it returns FALSE. Subsequent invocations will then return FALSE without doing anything.
    bool emitNextPage() {
      if (fileIndex_ >= entries_.size()) {
        if (subscriber_.has_value()) {
          // TODO: postpone duration measurement until all page _contents_ (i.e. inner observables) have been processed
          metrics_->dataRead_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count()); // in seconds
          subscriber_->on_completed();
          subscriber_.reset();
        }
        return false;
      }

      assert(subscriber_.has_value());

      auto& sfentry = entries_[fileIndex_];
      assert(sfentry->content() != nullptr);
      assert(sfentry->content()->payload() != nullptr);

      auto pageCount = sfentry->content()->payload()->pageCount();
      if (pageCount != 0U) {
        assert(pageIndex_ < pageCount); // Implies that pageCount != 0U

        subscriber_->on_next(sfentry->readPage(pageIndex_)
          .map([self = SharedFrom(*this), fileIndex = fileIndex_, pageIndex = pageIndex_](std::shared_ptr<std::string> contents) {
            // Schedule a followup page (observable) when caller is done processing this page's contents.
            // PEP_DEFER ensures that the outer observable keeps going even if we raise an exception (on the inner observable) from this lambda
            PEP_DEFER(self->emitNextPage());

            auto page = Serialization::FromString<DataPayloadPage>(*contents);
            page.index_ = fileIndex;
            page.pageNumber_ = pageIndex;

            auto returnedPage = std::make_shared<std::string>(
              Serialization::ToString(std::move(page)));
            if (returnedPage->size() >= messaging::MAX_SIZE_OF_MESSAGE) {
              throw std::runtime_error("Data payload page too large to send out");
            }
            self->metrics_->data_retrieved_bytes.Increment(
              static_cast<double>(returnedPage->size()));
            return returnedPage;
            }));
      }

      ++pageIndex_;
      if (pageIndex_ >= pageCount) {
        ++fileIndex_;
        pageIndex_ = 0U;
      }
      return true;
    }

  public:
    void emitTo(rxcpp::subscriber<messaging::MessageSequence> subscriber) {
      assert(fileIndex_ == 0U);
      assert(pageIndex_ == 0U);
      assert(!subscriber_.has_value());

      subscriber_ = subscriber;
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

  auto ctx = StreamContext::Create(std::move(entries), metrics_, time);

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
    auto certified = signedRequest->open(*rootCAs);
    auto request = MakeSharedCopy(std::move(certified.message));
    auto ticket = request->ticket_.open(*rootCAs, certified.signatory.organizationalUnit());

    if (!ticket.hasMode("write")) {
      throw Error("Ticket is missing \"write\" access mode");
    }

    struct StreamContext {
      std::vector<std::shared_ptr<FileStore::EntryChange>> entries;
      std::vector<std::shared_ptr<LocalPseudonym>> pseudonyms;
      std::vector<std::string> ids;
      std::vector<std::string> errors;
      std::vector<uint64_t> fileSizes;
      decltype(time) startTime;
    };
    auto ctx = std::make_shared<StreamContext>();

    ctx->entries.resize(request->entries_.size(), nullptr);
    ctx->pseudonyms.resize(request->entries_.size());
    ctx->ids.resize(request->entries_.size());
    ctx->fileSizes.resize(request->entries_.size());
    ctx->startTime = time;

    std::unordered_map<uint32_t, std::shared_ptr<LocalPseudonym>> pseudonymLut;
    for (size_t i = 0; i < request->entries_.size(); i++) {
      const auto& entry = request->entries_[i];

      // Decrypt local pseudonym
      if (pseudonymLut.count(entry.pseudonymIndex_) == 0) {
        pseudonymLut[entry.pseudonymIndex_] = MakeSharedCopy(
          ticket.accessSubjects.at(entry.pseudonymIndex_)
          .storageFacility.decrypt(pseudonymKey_));
      }
      ctx->pseudonyms[i] = pseudonymLut[entry.pseudonymIndex_];

      auto& col = ticket.columns.at(entry.columnIndex_);

      // Modify entry, only creating a new one if we don't require an overwrite
      auto entryChange = fileStore_->modifyEntry(EntryName(*ctx->pseudonyms[i], col), !requireContentOverwrite);
      if (entryChange == nullptr) {
        throw Error("Cannot find cell to update");
      }
      if (requireContentOverwrite && entryChange->isTombstone()) {
        throw Error("Cannot update cell that has been previously cleared/deleted");
      }

      for (size_t j = 0; j < i; ++j) { // TODO: improve performance: we don't want an inner loop making this O(n^2)
        if (ctx->entries[j]->getName() == entryChange->getName()) {
          PEP_LOG(LogTag, Severity::Error) << "Single request contained duplicate entry change for " + entryChange->getName().string();
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
              PEP_LOG(LogTag, Severity::Warning) << ss.str();
              ctx->errors.push_back(ss.str());
              // An exception will be raised by the call (below) to Serialization::FromString<DataPayloadPage>
            }

            auto page = Serialization::FromString<DataPayloadPage>(*rawPage);

            // Note that .at() tests bounds.
            auto& sfentry = ctx->entries.at(page.index_);
            auto fs = uint64_t{page.payloadData_.size()};
            ctx->fileSizes[page.index_] += fs;

            if (fs > 100000000) {
              throw Error("Incoming page is too large");
            }
            return sfentry->appendPage(rawPage, fs, page.pageNumber_).tap(
              [server, rawPage](const std::string& md5hash) {
                server->metrics_->data_stored_bytes.Increment(static_cast<double>(rawPage->size()));
              });
          })
          .as_dynamic()
          // We can't use merge here because the md5hashes need to be added to
          // the hasher in the correct order, so we use concat
          .op(RxParallelConcat(parallelisationWidth_))
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
                  PEP_LOG(LogTag, Severity::Warning) << ss.str();
                  ctx->errors.push_back(ss.str());
                }
              }

              server->updateFileStoreMetrics();

              if (!ctx->errors.empty()) {
                auto description = boost::algorithm::join(ctx->errors, "; ");
                subscriber.on_error(std::make_exception_ptr(Error(description)));
                // TODO: don't invoke on_next and on_completed anymore
              }

              subscriber.on_next(rxcpp::observable<>::from(
                MakeSharedCopy(getResponse(time, ctx->ids, hasher->digest()))));
              server->metrics_->dataStore_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx->startTime).count()); // in seconds
              subscriber.on_completed();
            });
      });
}

messaging::MessageBatches StorageFacility::handleDataStoreRequest2(
  std::shared_ptr<SignedDataStoreRequest2> signedRequest,
  messaging::MessageSequence tail) {
  auto getEntryContent = [filestore = fileStore_](const DataStoreEntry2& entry) {
    auto metadata = filestore->makeMetadataMap(entry.metadata_.extra());
    return std::make_unique<EntryContent>(
        std::move(metadata),
        EntryContent::PayloadData(
          { .polymorphicKey = entry.polymorphicKey_,
            .blindingTimestamp = entry.metadata_.getBlindingTimestamp(),
            .scheme = entry.metadata_.getEncryptionScheme()
          },
          nullptr));
  };

  auto getResponse = [](Timestamp /* ignored */, const std::vector<std::string>& ids, XxHasher::Hash hash) {
    DataStoreResponse2 resp;
    resp.ids_ = ids;
    resp.hash_ = hash;
    return Serialization::ToString(
      std::move(resp));
  };

  return this->handleDataAlterationRequest<DataStoreRequest2>(signedRequest, tail, false, getEntryContent, getResponse);
}

messaging::MessageBatches
StorageFacility::handleMetadataStoreRequest2(std::shared_ptr<SignedMetadataUpdateRequest2> lpRequest) {
  const auto& rootCAs = this->getRootCAs();
  auto certified = lpRequest->open(*rootCAs);
  auto request = MakeSharedCopy(std::move(certified.message));
  auto userGroup = certified.signatory.organizationalUnit();

  auto ticket = request->ticket_.open(*rootCAs, userGroup);

  if (!ticket.hasMode("write-meta")) {
    throw Error("Ticket is missing write-meta access mode");
  }

  // Fill a vector with indices of pseudonyms that we want/need decrypted
  std::vector<uint32_t> pseudIndices;
  pseudIndices.reserve(request->entries_.size());
  std::transform(request->entries_.cbegin(), request->entries_.cend(), std::back_inserter(pseudIndices), [](const DataStoreEntry2& entry) {return entry.pseudonymIndex_; });

  // Decrypt pseudonyms.
  auto localPseudonyms = this->decryptLocalPseudonyms(ticket.accessSubjects, &pseudIndices);

  std::vector<std::shared_ptr<FileStore::EntryChange>> changes;
  for (const auto& entry : request->entries_) {
    auto column = ticket.columns[entry.columnIndex_];
    assert(localPseudonyms[entry.pseudonymIndex_].has_value());
    EntryName key(*localPseudonyms[entry.pseudonymIndex_], column);

    auto entryChange = fileStore_->modifyEntry(key, false);
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
        fileStore_->makeMetadataMap(entry.metadata_.extra()),
        EntryContent::PayloadData{
          {
            .polymorphicKey = entry.polymorphicKey_,
            .blindingTimestamp = entry.metadata_.getBlindingTimestamp(),
            .scheme = entry.metadata_.getEncryptionScheme()
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
    response.ids_.push_back(id);
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
      .timestamp_ = timestamp,
      .entries_{}, // Filled below
    };

    resp.entries_.indices_.reserve(ids.size());
    for (size_t i = 0U; i < ids.size(); ++i) {
      if (!ids[i].empty()) {
        resp.entries_.indices_.emplace_back(static_cast<uint32_t>(i));
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
      result.emplace_back(source[i].storageFacility.decrypt(pseudonymKey_));
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
  PEP_LOG(LogTag, Severity::Debug) << "Received DataHistoryRequest2";

  auto start_time = std::chrono::steady_clock::now();
  const auto& rootCAs = this->getRootCAs();
  auto certified = lpRequest->open(*rootCAs);
  const auto& request = certified.message;

  auto accessGroup = certified.signatory.organizationalUnit();
  UserGroup::EnsureAccess({UserGroup::DataAdministrator, UserGroup::Watchdog}, accessGroup);

  auto ticket = request.ticket_.open(*rootCAs, accessGroup, "read-meta");

  DataHistoryResponse2 response;

  // Look-up table to check whether to include column
  std::vector<std::string> includeColumn;
  if (request.columns_) {
    includeColumn.reserve(request.columns_->indices_.size());
    for (uint32_t idx : request.columns_->indices_)
      includeColumn.push_back(ticket.columns.at(idx));
  }
  else {
    includeColumn.reserve(ticket.columns.size());
    for (const auto& column : ticket.columns)
      includeColumn.push_back(column);
  }

  // Create column-to-ticket-column-index look-up-table
  std::unordered_map<std::string, uint32_t> columnIndex;
  columnIndex.reserve(ticket.columns.size());
  for (uint32_t i = 0; i < ticket.columns.size(); i++) {
    columnIndex[ticket.columns[i]] = i;
  }
  // Decrypt pseudonyms.
  auto localPseudonyms = this->decryptLocalPseudonyms(ticket.accessSubjects, request.pseudonyms_.has_value() ? &request.pseudonyms_->indices_ : nullptr);

  std::vector<uint64_t> ids; // used to lookup id from responseEntry index_
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
      auto history = fileStore_->lookupWithHistory(key);
      for (const auto& entry : history) {
        assert(entry != nullptr);

        Timestamp validFrom = entry->getValidFrom();
        response.entries_.push_back({
          .columnIndex_ = colIndexIt->second,
          .pseudonymIndex_ = static_cast<uint32_t>(pseud_index),
          .timestamp_ = validFrom,
          .id_ = !entry->isTombstone() ? encryptId(entry->getName().string(), validFrom) : "",
        });
      }
    }
  }

  metrics_->dataHistory_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); // in seconds

  return rxcpp::observable<>::just(
    rxcpp::observable<>::just(MakeSharedCopy(Serialization::ToString(response))).as_dynamic());
}

messaging::MessageBatches StorageFacility::handleDataSizeRequest(std::shared_ptr<SignedDataSizeRequest> lpRequest) {
  const auto& rootCAs = this->getRootCAs();
  auto certified = lpRequest->open(*rootCAs);

  auto accessGroup = certified.signatory.organizationalUnit();
  UserGroup::EnsureAccess({ UserGroup::DataAdministrator }, accessGroup);

  const auto& request = certified.message;

  size_t entryCount{};
  uint64_t totalBytes{}, rollingBytes{};
  this->getFileStoreMetrics(entryCount, totalBytes, rollingBytes, request.columns);

  auto countBlocks = [blockSize = dataSizeResolution_](uint64_t bytes) {
      assert(bytes % blockSize == 0U);
      return bytes / blockSize;
    };

  return messaging::BatchSingleMessage(DataSizeResponse{
    .blockSize = dataSizeResolution_,
    .totalBlocks = countBlocks(totalBytes),
    .rollingBlocks = countBlocks(rollingBytes),
    });
}

std::string StorageFacility::encryptId(std::string path, Timestamp time) {
  return Serialization::ToString(
    EncryptedSFId(
      encIdKey_,
      SFId{std::move(path), time}),
    false);
}

SFId StorageFacility::decryptId(std::string_view encId) {
  return Serialization::FromString<EncryptedSFId>(encId, false).decrypt(encIdKey_);
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
  result.extra() = fileStore_->extractMetadataMap(content->metadata());

  return result;
}

std::optional<std::filesystem::path> StorageFacility::getStoragePath() {
  return EnsureDirectoryPath(fileStore_->metaDir());
}

void StorageFacility::statsTimer(const boost::system::error_code& e) {
  if (e == boost::asio::error::operation_aborted) {
    return;
  }
  auto metaDirs = std::filesystem::directory_iterator(fileStore_->metaDir());
  auto metaDirsCount = std::distance(metaDirs, std::filesystem::directory_iterator());

  metrics_->entriesInMetaDir.Set(static_cast<double>(metaDirsCount));

  timer_.expires_after(60s);
  timer_.async_wait(boost::bind(&pep::StorageFacility::statsTimer, this, boost::asio::placeholders::error));
}

StorageFacility::StorageFacility(std::shared_ptr<pep::StorageFacility::Parameters> parameters)
  : SigningServer(parameters),
  pseudonymKey_(parameters->getPseudonymKey()), encIdKey_(parameters->getEncIdKey()),
  workerPool_(WorkerPool::getShared()),
  fileStore_(FileStore::Create(
    parameters->getStoragePath().string(),
    *parameters->getPageStoreConfig(),
    parameters->getIoContext(),
    registry_)),
  metrics_(std::make_shared<Metrics>(registry_)),
  timer_(*parameters->getIoContext()),
  parallelisationWidth_(parameters->getParallelisationWidth()),
  dataSizeResolution_(parameters->getDataSizeResolution()) {
  RegisterRequestHandlers(*this,
                          &StorageFacility::handleMetadataReadRequest2,
                          &StorageFacility::handleDataReadRequest2,
                          &StorageFacility::handleDataStoreRequest2,
                          &StorageFacility::handleDataDeleteRequest2,
                          &StorageFacility::handleMetadataStoreRequest2,
                          &StorageFacility::handleDataEnumerationRequest2,
                          &StorageFacility::handleDataHistoryRequest2,
                          &StorageFacility::handleDataSizeRequest);

  this->updateFileStoreMetrics();
  statsTimer({});
}

void StorageFacility::getFileStoreMetrics(size_t& entryCount, uint64_t& roundedTotalBytes, uint64_t& roundedRollingBytes, const std::set<std::string>& columns) {
  uint64_t total{}, rolling{};
  fileStore_->getMetrics(entryCount, total, rolling, columns);

  auto round = [blockSize = dataSizeResolution_](uint64_t bytes) {
      auto blocks = bytes / blockSize;
      if (bytes % blockSize != 0U) {
        ++blocks;
      }
      return blocks * blockSize;
    };

  roundedTotalBytes = round(total);
  roundedRollingBytes = round(rolling);
}

void StorageFacility::updateFileStoreMetrics() {
  size_t entryCount{};
  uint64_t totalPayloadBytes{}, rollingPayloadBytes{};
  this->getFileStoreMetrics(entryCount, totalPayloadBytes, rollingPayloadBytes);

  metrics_->entriesIncludingHistory.Set(static_cast<double>(entryCount));
  metrics_->totalPayloadBytes.Set(static_cast<double>(totalPayloadBytes));
  metrics_->rollingPayloadBytes.Set(static_cast<double>(rollingPayloadBytes));
}

}
