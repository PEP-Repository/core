#include <pep/core-client/CoreClient.hpp>
#include <pep/async/RxConcatenateVectors.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/storagefacility/PageHash.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

namespace {

const std::string LogTag("CoreClient.Data.Write");

constexpr unsigned METADATA_UPDATE_BATCH_SIZE = 2500;

}

StoreData2Entry::StoreData2Entry(
  std::shared_ptr<PolymorphicPseudonym> pp,
  std::string column,
  std::shared_ptr<std::string> data,
  const std::vector<NamedMetadataXEntry>& xentries)
  : StoreData2Entry(pp, std::move(column), rxcpp::observable<>::just(rxcpp::observable<>::just(data).as_dynamic())) {
  for (const auto& xentry : xentries) {
    auto emplaced = xMetadata_.emplace(xentry).second;
    if (!emplaced) {
      throw std::runtime_error("Duplicate metadata entry name specified: " + xentry.first);
    }
  }
}

rxcpp::observable<DataStorageResult2> CoreClient::storeData2(
    const PolymorphicPseudonym& pp,
    const std::string& column,
    std::shared_ptr<std::string> data,
    const std::vector<NamedMetadataXEntry>& xentries,
    const StoreData2Opts& opts) {
  return storeData2(
    {StoreData2Entry(
        std::make_shared<PolymorphicPseudonym>(pp),
        column,
        data,
        xentries
    )},
    opts
  );
}

rxcpp::observable<DataStorageResult2> CoreClient::storeData2(
    const std::vector<StoreData2Entry>& entries,
    const StoreData2Opts& opts) {
  PEP_LOG(LogTag, Severity::Debug) << "storeData";

  struct Context {
    std::unordered_map<std::string,uint32_t> columns;
    std::unordered_map<PolymorphicPseudonym,uint32_t> pps;
    std::shared_ptr<DataStoreRequest2> request;
    std::vector<AESKey> keys;
    std::vector<messaging::MessageBatches> data;
  };
  auto ctx = std::make_shared<Context>();

  // generate AES keys
  ctx->keys = std::vector<AESKey>(entries.size()); // the default constructor of AESKey generates a random key

  // Create ticket request
  RequestTicket2Opts ticketRequest;
  ticketRequest.ticket = opts.ticket;
  ticketRequest.forceTicket = opts.forceTicket;
  ticketRequest.modes = {"write"};
  for (const auto& entry : entries) {
    if (ctx->columns.count(entry.column_) == 0) {
      ctx->columns[entry.column_] = static_cast<uint32_t>(
          ticketRequest.columns.size());
      ticketRequest.columns.push_back(entry.column_);
    }
    if (ctx->pps.count(*entry.polymorphicPseudonym_) == 0) {
      ctx->pps[*entry.polymorphicPseudonym_] = static_cast<uint32_t>(
          ticketRequest.pps.size());
      ticketRequest.pps.push_back(*entry.polymorphicPseudonym_);
    }
  }

  // Construct request to storage facility already to
  // prevent another copy of entries.
  ctx->request = std::make_shared<DataStoreRequest2>();
  ctx->request->entries_.reserve(entries.size());
  ctx->data.reserve(entries.size());

  for (size_t i=0; i<entries.size(); i++) {
    const auto& entry = entries.at(i);

    DataStoreEntry2 entry2;
    entry2.columnIndex_ = ctx->columns[entry.column_];
    entry2.pseudonymIndex_ = ctx->pps[*entry.polymorphicPseudonym_];
    entry2.metadata_ = Metadata(entry.column_,
      entry.timestamp_ ? *entry.timestamp_ : TimeNow());

    // set extra metadata entries, encrypting them with the entry's key
    // when requested.
    for (auto&& [name, xentry] : entry.xMetadata_) {
      entry2.metadata_.extra()[name] = xentry.prepareForStore(ctx->keys[i].bytes);
    }

    ctx->request->entries_.emplace_back(entry2);

    ctx->data.push_back(entry.batches_);
  }

  // Send ticket request
  auto requestedPps = ticketRequest.pps.size();
  return requestTicket2(ticketRequest)
  .flat_map([this,ctx, requestedPps](IndexedTicket2 indexedTicket) {
    auto signedTicket = std::move(indexedTicket).getTicket();
    ctx->request->ticket_ = *signedTicket;

    const auto accessSubjectCount = signedTicket->openWithoutCheckingSignature().accessSubjects_.size();
    if (accessSubjectCount < requestedPps) {
      std::ostringstream msg;
      msg << "Received ticket for " << accessSubjectCount << " subject(s) but requested access to " << requestedPps;
      PEP_LOG(LogTag, Severity::Error) << msg.str();
      throw std::runtime_error(msg.str());
    }

    return this->encryptAndBlindKeys(ctx->request, ctx->keys);
  })
    .op(RxGetOne("key encryption and blinding result"))
    .flat_map([this,ctx](FakeVoid) {
    // so we get a observable (obs) of a obs of a obs of strings here, so a obs^3(string) (due to interfaces)
    // we need a obs^2(string)
    // we use a outer merge of obs^2 here, as the inner obs^2's should not be merge (because of ordering that needs to be intact due to the lambda used below)
    // (no a concat on the ineer obs^2 can also NOT be used, due to loading the file at once)
    auto pages = CreateObservable<messaging::Tail<DataPayloadPage>>([ctx](rxcpp::subscriber<messaging::Tail<DataPayloadPage>> subscriber) {
      for (size_t i = 0; i < ctx->request->entries_.size(); ++i) {
        subscriber.on_next(ctx->data[i].map([i, ctx, fileContext = std::make_shared<uint64_t>()](messaging::MessageSequence obs) -> messaging::TailSegment<DataPayloadPage> {
          return obs.map([i, ctx, fileContext](std::shared_ptr<std::string> in) {
            DataPayloadPage page;
            page.pageNumber_ = (*fileContext)++;
            page.index_ = static_cast<uint32_t>(i);
            page.setEncrypted(
                  *in,
                  ctx->keys[i].bytes,
                  ctx->request->entries_[i].metadata_
                  );
            return page;
            });
        }));
      }
      subscriber.on_completed();
    }).merge(); // look at comment above for reasoning about the location of this merge

    return getStorageFacilityProxy(true)->requestDataStore(*ctx->request, pages);
  }).map([ctx](DataStoreResponse2 response) {
    DataStorageResult2 result;
    result.ids_ = response.ids_;
    return result;
  });
}

rxcpp::observable<DataStorageResult2> CoreClient::updateMetadata2(
  const std::vector<StoreMetadata2Entry>& entries,
  const StoreData2Opts& opts) { // TODO: consolidate duplicate code with deleteData2 method (below)
  PEP_LOG(LogTag, Severity::Debug) << "updateMetadata";

  struct Context {
    std::unordered_map<std::string, uint32_t> columns;
    std::unordered_map<PolymorphicPseudonym, uint32_t> pps;
    std::shared_ptr<MetadataUpdateRequest2> request;
    std::shared_ptr<TicketPseudonyms> pseudonyms;
  };
  auto ctx = std::make_shared<Context>();

  // Construct request to storage facility already to
  // prevent another copy of entries.
  ctx->request = std::make_shared<MetadataUpdateRequest2>();
  ctx->request->entries_.reserve(entries.size());

  // Create ticket request
  RequestTicket2Opts ticketRequest;
  ticketRequest.ticket = opts.ticket;
  ticketRequest.forceTicket = opts.forceTicket;
  ticketRequest.modes = { "read", "write-meta" }; // We need read access so that we can re-encrypt-and-blind the AES keys
  for (const auto& entry : entries) {
    // Ensure that the ticket contains this entry's column(name) and PP, but
    // don't add duplicate items. We keep track of indices where the ticket
    // specifies the column(name)s and PPs , allowing us to easily/speedily find
    // the associated indices when we construct the DataStoreEntry2 (below).

    if (ctx->columns.count(entry.column_) == 0) {
      ctx->columns[entry.column_] = static_cast<uint32_t>(
        ticketRequest.columns.size()); // Associate the column(name) with the index it'll get in the ticket
      ticketRequest.columns.push_back(entry.column_);
    }
    if (ctx->pps.count(*entry.polymorphicPseudonym_) == 0) {
      ctx->pps[*entry.polymorphicPseudonym_] = static_cast<uint32_t>(
        ticketRequest.pps.size()); // Associate the PP with the index it'll get in the ticket
      ticketRequest.pps.push_back(*entry.polymorphicPseudonym_);
    }

    DataStoreEntry2 storeEntry2;
    storeEntry2.columnIndex_ = ctx->columns[entry.column_];
    storeEntry2.pseudonymIndex_ = ctx->pps[*entry.polymorphicPseudonym_];
    storeEntry2.metadata_ = Metadata(entry.column_,
      entry.timestamp_ ? *entry.timestamp_ : TimeNow());
    // storeEntry2.polymorphicKey_ is set later, once we have retrieved it
    storeEntry2.metadata_.extra() = entry.xMetadata_; // These are encrypted later, once we have retrieved the keys

    ctx->request->entries_.emplace_back(storeEntry2);
  }

  auto requestedPps = ticketRequest.pps.size();
  return requestTicket2(ticketRequest) // Request a ticket
    .flat_map([this, ctx, requestedPps](IndexedTicket2 indexedTicket) {
    auto signedTicket = std::move(indexedTicket).getTicket();
    ctx->request->ticket_ = *signedTicket;
    ctx->pseudonyms = std::make_shared<TicketPseudonyms>(*signedTicket, privateKeyPseudonyms_);

    auto accessSubjectCount = signedTicket->openWithoutCheckingSignature().accessSubjects_.size();
    if (accessSubjectCount < requestedPps) {
      std::ostringstream msg;
      msg << "Received ticket for " << accessSubjectCount << " subject(s) but requested access to " << requestedPps;
      PEP_LOG(LogTag, Severity::Error) << msg.str();
      throw std::runtime_error(msg.str());
    }

    // Get previous data (including polymorphic key) for the entries whose metadata we're going to update
    DataEnumerationRequest2 enumRequest;
    enumRequest.ticket_ = *signedTicket;
    enumRequest.columns_ = IndexList();
    enumRequest.columns_->indices_.reserve(ctx->columns.size());
    std::transform(ctx->columns.cbegin(), ctx->columns.cend(), std::back_inserter(enumRequest.columns_->indices_), [](const std::pair<const std::string, uint32_t>& pair) {return pair.second; });
    enumRequest.pseudonyms_ = IndexList();
    enumRequest.pseudonyms_->indices_.reserve(ctx->pps.size());
    std::transform(ctx->pps.cbegin(), ctx->pps.cend(), std::back_inserter(enumRequest.pseudonyms_->indices_), [](const std::pair<const PolymorphicPseudonym, uint32_t>& pair) {return pair.second; });

    return this->getStorageFacilityProxy(true)->requestDataEnumeration(std::move(enumRequest))
      .map([ctx](const DataEnumerationResponse2& response) { return response.entries_; })
      .op(RxConcatenateVectors())
      .flat_map([this, ctx, signedTicket](std::shared_ptr<std::vector<DataEnumerationEntry2>> enumEntries) {
      if (enumEntries->size() < ctx->request->entries_.size()) {
        throw std::runtime_error("Could not find all entries for metadata update. Attempting to update deleted entries?");
      }

      // Allow speedy lookup of columnIndex -> pseudonymIndex -> indexInEnumEntries
      std::unordered_map<uint32_t, std::unordered_map<uint32_t, size_t>> enumEntryIndices;
      enumEntryIndices.reserve(enumEntries->size());
      for (size_t i = 0U; i < enumEntries->size(); ++i) {
        const auto& enumEntry = (*enumEntries)[i];
        auto& pseudIndices = enumEntryIndices[enumEntry.columnIndex_]; // Get (reference to) an unordered_map for this column index
        auto emplaced [[maybe_unused]] = pseudIndices.emplace(std::make_pair(enumEntry.pseudonymIndex_, i)).second;
        assert(emplaced); // or we have received multiple enumeration entries for the same columnIndex+pseudonymIndex combination
      }

      // We have all the current (meta)data and polymorphic keys for the entries whose metadata we're going to update. Decrypt the keys now...
      return this->unblindAndDecryptKeys(ConvertDataEnumerationEntries(std::move(*enumEntries), *ctx->pseudonyms), signedTicket)
        .flat_map([this, ctx, enumEntryIndices = PtrAsConst(MakeSharedCopy(std::move(enumEntryIndices)))
          ](const std::vector<AESKey>& enumKeys) { // ... then use the decrypted keys to prepare our request entries...
        if (enumKeys.size() != enumEntryIndices->size()) {
          throw std::runtime_error("Received unexpected number of plaintext keys");
        }

        // Update every DataStoreEntry2 with properties from the enum entry representing the data card that we'll overwrite
        std::vector<AESKey> storeKeys;
        storeKeys.reserve(ctx->request->entries_.size());
        for (auto& storeEntry : ctx->request->entries_) {
          // Find the enum entry corresponding with this store entry
          auto correspondingColumn = enumEntryIndices->find(storeEntry.columnIndex_);
          if (correspondingColumn == enumEntryIndices->cend()) {
            throw std::runtime_error("Did not receive existing entry for metadata update for column " + ctx->request->ticket_.openWithoutCheckingSignature().columns_[storeEntry.columnIndex_]);
          }
          auto correspondingEnumEntry = correspondingColumn->second.find(storeEntry.pseudonymIndex_);
          if (correspondingEnumEntry == correspondingColumn->second.cend()) {
            auto ticket = ctx->request->ticket_.openWithoutCheckingSignature();
            std::ostringstream message;
            message << "Did not receive existing entry for metadata update"
              << " for participant " << ticket.accessSubjects_[storeEntry.pseudonymIndex_].polymorphic_.text()
              << ", column " << ticket.columns_[storeEntry.columnIndex_];
            throw std::runtime_error(message.str());
          }

          // Find the AES key and associate it (by index) with our store entry
          auto index = correspondingEnumEntry->second;
          const auto& aes = enumKeys[index];
          storeKeys.emplace_back(aes);

          // Initialize the store entry's metadata using the AES key
          for (auto& keyValuePair : storeEntry.metadata_.extra()) {
            auto& xEntry = keyValuePair.second;
            xEntry = xEntry.prepareForStore(aes.bytes);
          }
        }

        return this->encryptAndBlindKeys(ctx->request, storeKeys); // ... then (encrypt and) blind the keys on the basis of the updated metadata
          })
        .as_dynamic() // Reduce compiler memory usage
        .op(RxGetOne("key encryption and blinding result"))
        .flat_map([this, ctx](FakeVoid) {
        // (The entries in) our MetadataUpdateRequest2 are now ready: split them over multiple requests to prevent individual messages from becoming too large for our network layer
        std::unordered_map<size_t, std::shared_ptr<MetadataUpdateRequest2>> batches;
        batches.reserve(ctx->request->entries_.size() / METADATA_UPDATE_BATCH_SIZE + 1);
        for (size_t i = 0U; i < ctx->request->entries_.size(); ++i) {
          auto indexInBatch = i % METADATA_UPDATE_BATCH_SIZE;
          auto offset = i - indexInBatch;
          assert(offset % METADATA_UPDATE_BATCH_SIZE == 0U);
          if (batches[offset] == nullptr) {
            batches[offset] = std::make_shared<MetadataUpdateRequest2>();
            batches[offset]->ticket_ = ctx->request->ticket_;
          }
          assert(batches[offset]->entries_.size() == indexInBatch);
          batches[offset]->entries_.emplace_back(ctx->request->entries_[i]);
        }

        // Send individual MetadataUpdateRequest2 instances to Storage Facility
        return RxIterate(std::move(batches))
          .flat_map([this](const std::pair<const size_t, std::shared_ptr<MetadataUpdateRequest2>>& pair) {
          size_t offset = pair.first;
          std::shared_ptr<MetadataUpdateRequest2> request = pair.second;
          return getStorageFacilityProxy(true)->requestMetadataStore(*request)
            .map([offset](MetadataUpdateResponse2 response) {
              return std::make_pair(offset, std::move(response));
            })
            .as_dynamic(); // Reduce compiler memory usage
          })
          .reduce( // Collect individual MetadataUpdateResponse2 instances into a single DataStorageResult2
            std::make_shared<DataStorageResult2>(),
            [](std::shared_ptr<DataStorageResult2> result, const std::pair<size_t, MetadataUpdateResponse2>& batch) {
              size_t offset = batch.first;
              const MetadataUpdateResponse2& response = batch.second;
              result->ids_.resize(std::max(result->ids_.size(), offset + response.ids_.size()));
              for (size_t i = 0U; i < response.ids_.size(); ++i) {
                result->ids_[offset + i] = response.ids_[i];
              }
              return result;
            })
          .map([](std::shared_ptr<DataStorageResult2> result) {
            assert(std::all_of(result->ids_.cbegin(), result->ids_.cend(), [](const std::string& id) {return !id.empty(); }));
            return *result;
            }).as_dynamic();
          }).as_dynamic(); // Reduce compiler memory usage
        }).as_dynamic(); // Reduce compiler memory usage
      });
}

rxcpp::observable<HistoryResult> CoreClient::deleteData2(
  const PolymorphicPseudonym& pp,
  const std::string& column,
  const StoreData2Opts& opts) {
  return deleteData2(
    { Storage2Entry(
        std::make_shared<PolymorphicPseudonym>(pp),
        column
    ) },
    opts
  );

}
rxcpp::observable<HistoryResult> CoreClient::deleteData2(
  const std::vector<Storage2Entry>& entries,
  const StoreData2Opts& opts) { // TODO: consolidate duplicate code with storeData2 method (above)
  PEP_LOG(LogTag, Severity::Debug) << "deleteData";

  struct Context {
    std::unordered_map<std::string, uint32_t> columns;
    std::unordered_map<PolymorphicPseudonym, uint32_t> pps;
    std::shared_ptr<DataDeleteRequest2> request;
  };
  auto ctx = std::make_shared<Context>();

  // Create ticket request
  RequestTicket2Opts ticketRequest;
  ticketRequest.ticket = opts.ticket;
  ticketRequest.forceTicket = opts.forceTicket;
  ticketRequest.modes = { "write" };
  for (const auto& entry : entries) {
    if (ctx->columns.count(entry.column_) == 0) {
      ctx->columns[entry.column_] = static_cast<uint32_t>(
        ticketRequest.columns.size());
      ticketRequest.columns.push_back(entry.column_);
    }
    if (ctx->pps.count(*entry.polymorphicPseudonym_) == 0) {
      ctx->pps[*entry.polymorphicPseudonym_] = static_cast<uint32_t>(
        ticketRequest.pps.size());
      ticketRequest.pps.push_back(*entry.polymorphicPseudonym_);
    }
  }

  // Construct request to storage facility already to
  // prevent another copy of entries.
  ctx->request = std::make_shared<DataDeleteRequest2>();
  ctx->request->entries_.reserve(entries.size());

  for (const auto &entry: entries) {
    DataStoreEntry2 entry2;
    entry2.columnIndex_ = ctx->columns[entry.column_];
    entry2.pseudonymIndex_ = ctx->pps[*entry.polymorphicPseudonym_];
    ctx->request->entries_.emplace_back(entry2);
  }

  auto requestedPps = ticketRequest.pps.size();
  return requestTicket2(ticketRequest)
    .flat_map([this, ctx, requestedPps](IndexedTicket2 indexedTicket) {
    auto signedTicket = std::move(indexedTicket).getTicket();
    ctx->request->ticket_ = *signedTicket;

    auto accessSubjectCount = signedTicket->openWithoutCheckingSignature().accessSubjects_.size();
    if (accessSubjectCount < requestedPps) {
      std::ostringstream msg;
      msg << "Received ticket for " << accessSubjectCount << " subject(s) but requested access to " << requestedPps;
      PEP_LOG(LogTag, Severity::Error) << msg.str();
      throw std::runtime_error(msg.str());
    }

    return getStorageFacilityProxy(true)->requestDataDelete(*ctx->request)
      .flat_map([this, ctx](const DataDeleteResponse2& response) {
      auto ticket = ctx->request->ticket_.openWithoutCheckingSignature();
      // TODO: use CreateObservable instead of rxcpp::iterate over a vector<> that we just create for this purpose
      std::vector<std::shared_ptr<LocalPseudonyms>> pseudonyms;
      pseudonyms.reserve(ticket.accessSubjects_.size());
      std::optional<bool> includeAccessGroupPseudonyms;
      std::vector<std::shared_ptr<LocalPseudonym>> agPseuds;
      for (const auto& p : ticket.accessSubjects_) {
        pseudonyms.push_back(std::make_shared<LocalPseudonyms>(p));
        if (includeAccessGroupPseudonyms.has_value()) {
          if (p.accessGroup_.has_value() != *includeAccessGroupPseudonyms) {
            throw std::runtime_error("Inconsistent access group pseudonym presence in ticket");
          }
        }
        else {
          includeAccessGroupPseudonyms = p.accessGroup_.has_value();
        }
        if (*includeAccessGroupPseudonyms) {
          agPseuds.push_back(
            std::make_shared<LocalPseudonym>(
              p.accessGroup_->decrypt(privateKeyPseudonyms_)
              )
          );
        }
      }

      std::vector<HistoryResult> ress;

      ress.reserve(response.entries_.indices_.size());
      for (auto i : response.entries_.indices_) {
        const auto& requestEntry = ctx->request->entries_[i];
        ress.push_back(HistoryResult{
          DataCellResult{
            .localPseudonyms_ = pseudonyms[requestEntry.pseudonymIndex_],
            .localPseudonymsIndex_ = requestEntry.pseudonymIndex_,
            .column_ = ticket.columns_[requestEntry.columnIndex_],
            .accessGroupPseudonym_ = includeAccessGroupPseudonyms.value_or(false)
                                       ? agPseuds[requestEntry.pseudonymIndex_]
                                       : nullptr,
          },
          response.timestamp_,
        });
      }
      return RxIterate(std::move(ress));
    });
  });
}

}
