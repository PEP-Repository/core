#include <pep/core-client/CoreClient.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxConcatenateVectors.hpp>
#include <pep/async/RxIndexed.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>

#include <rxcpp/operators/rx-buffer_count.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-group_by.hpp>
#include <rxcpp/operators/rx-take.hpp>
#include <rxcpp/operators/rx-zip.hpp>

namespace pep {

namespace {

const std::string LOG_TAG("CoreClient.Data.Read");

template <typename TTicketItem, typename TSpecifiedItem>
void FillHistoryRequestIndices(const SignedTicket2& ticket,
  std::optional<Ticket2>& unsignedTicket,
  std::vector<TTicketItem> Ticket2::* ticketItemsMember,
  const std::optional<std::vector<TSpecifiedItem>>& specified,
  std::optional<IndexList>& indexList,
  const std::function<bool(const TTicketItem&, const TSpecifiedItem)>& itemsMatch) {
  if (specified != std::nullopt) {
    // Ensure that we have an unpacked Ticket2 to work with: either (previously unpacked and) provided by caller...
    if (!unsignedTicket.has_value()) {
      unsignedTicket = ticket.openWithoutCheckingSignature(); // ... or we unpack it now
    }
    std::vector<TTicketItem>& ticketItems = (*unsignedTicket).*ticketItemsMember;
    indexList = IndexList();
    for (const auto& specifiedItem : *specified) {
      auto position = std::find_if(ticketItems.cbegin(), ticketItems.cend(), [&specifiedItem, &itemsMatch](const TTicketItem& ticketItem) {
        return itemsMatch(ticketItem, specifiedItem);
        });
      if (position >= ticketItems.cend()) {
        throw std::runtime_error("Ticket does not provide access to the specified item");
      }
      auto index = static_cast<uint32_t>(position - ticketItems.cbegin());
      indexList->mIndices.emplace_back(index);
    }
  }
}

}

rxcpp::observable<std::vector<EnumerateResult>> CoreClient::enumerateData2(const std::vector<std::string>&
                                                                           participantGroups,
                                                                       const std::vector<PolymorphicPseudonym>& pps,
                                                                       const std::vector<std::string>& columnGroups,
                                                                       const std::vector<std::string>& columns) {
  return clientAccessManager
      ->sendRequest<SignedTicket2>(sign(TicketRequest2{.mModes = {"read"},
                                                       .mParticipantGroups = participantGroups,
                                                       .mPolymorphicPseudonyms = pps,
                                                       .mColumnGroups = columnGroups,
                                                       .mColumns = columns,
                                                       .mIncludeUserGroupPseudonyms = false}))
      .flat_map([this](SignedTicket2 ticket) { return this->enumerateData2(std::make_shared<SignedTicket2>(ticket)); });
}

rxcpp::observable<std::vector<EnumerateResult>> CoreClient::enumerateData2(std::shared_ptr<SignedTicket2> ticket) {
  LOG(LOG_TAG, debug) << "enumerateData";

  auto pseudonyms = std::make_shared<TicketPseudonyms>(*ticket, privateKeyPseudonyms);
  auto enumRequest = std::make_shared<DataEnumerationRequest2>();
  enumRequest->mTicket = *ticket;
  return clientStorageFacility->sendRequest(std::make_shared<std::string>(Serialization::ToString(
    sign(*enumRequest))))
    .map([this, pseudonyms](std::string rawResponse) {
      auto response = Serialization::FromString<
        DataEnumerationResponse2>(std::move(rawResponse));
      return convertDataEnumerationEntries(response.mEntries, *pseudonyms);
    });
}

rxcpp::observable<EnumerateResult>
CoreClient::getMetadata(const std::vector<std::string>& ids, std::shared_ptr<SignedTicket2> ticket) {
  LOG(LOG_TAG, debug) << "getMetadata";

  if (ids.empty()) {
    return rxcpp::observable<>::empty<EnumerateResult>();
  }

  std::vector<std::vector<std::string>> batches;
  for (size_t i = 0U; i < ids.size(); i += DATA_RETRIEVAL_BATCH_SIZE) {
    auto batchSize = std::min(ids.size() - i, DATA_RETRIEVAL_BATCH_SIZE);

    assert(i <= static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max())); // Ensure that we don't lose data in static_cast
    auto begin = ids.begin() + static_cast<ptrdiff_t>(i);

    static_assert(DATA_RETRIEVAL_BATCH_SIZE <= static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max())); // Ensure that we don't lose data in static_cast. We don't need a run time "assert(batchSize <= ...)" because batchSize <= DATA_RETRIEVAL_BATCH_SIZE
    auto end = begin + static_cast<ptrdiff_t>(batchSize);

    batches.emplace_back(std::vector<std::string>(begin, end));
  }

  auto pseudonyms = std::make_shared<TicketPseudonyms>(*ticket, privateKeyPseudonyms);
  return rxcpp::observable<>::iterate(std::move(batches))
    .flat_map([this, ticket, pseudonyms](std::vector<std::string> batch) {
    auto entryCount = batch.size();
    MetadataReadRequest2 readRequest;
    readRequest.mIds = std::move(batch);
    readRequest.mTicket = *ticket;
    return this->clientStorageFacility->sendRequest(
      std::make_shared<std::string>(Serialization::ToString(sign( readRequest))))
      .map([](std::string rawResponse) {
      auto response = Serialization::FromString<
        DataEnumerationResponse2>(std::move(rawResponse));
      return std::move(response.mEntries);
        })
      .op(RxConcatenateVectors())
      .flat_map([this, entryCount, pseudonyms](const std::shared_ptr<std::vector<DataEnumerationEntry2>>& entries) {
      if (entries->size() != entryCount) {
        throw std::runtime_error("Storage facility return an unexpected number of entries");
      }
      return rxcpp::observable<>::iterate(convertDataEnumerationEntries(*entries, *pseudonyms));
        });
      });
}

rxcpp::observable<std::shared_ptr<RetrieveResult>>
CoreClient::retrieveData2(
  const rxcpp::observable<EnumerateResult>& subjects,
  std::shared_ptr<SignedTicket2> ticket,
  bool includeContent) {
  LOG(LOG_TAG, debug) << "retrieveData";

  return subjects
    .buffer(static_cast<int>(DATA_RETRIEVAL_BATCH_SIZE))
    .as_dynamic() // Reduce compiler memory usage
    .op(RxIndexed<uint32_t>())
    .flat_map([this, ticket, includeContent](std::pair<uint32_t, std::vector<EnumerateResult>> subjectsBatch) {
      auto& [batchNum, subjects] = subjectsBatch;
      const uint32_t offset = batchNum * DATA_RETRIEVAL_BATCH_SIZE;
      struct fileContext {
        std::string key;
        uint64_t written = 0;
      };
      struct context {
        std::vector<fileContext> files;
        std::vector<EnumerateResult> subjects;
      };
      auto ctx = std::make_shared<context>();
      ctx->subjects = std::move(subjects);

      return this->unblindAndDecryptKeys(ctx->subjects, ticket) // Get keys
        .op(RxConcatenateVectors())
        .flat_map([this, ctx, ticket, includeContent, offset](const std::shared_ptr<std::vector<AESKey>>& keys)
          -> rxcpp::observable<std::shared_ptr<RetrieveResult>> {
          if (keys->size() != ctx->subjects.size())
            throw std::runtime_error("KeyResponse contains the wrong number of entries");
          ctx->files.reserve(keys->size());
          std::transform(keys->begin(), keys->end(), std::back_inserter(ctx->files),
                         [](AESKey& key) { return fileContext{std::move(key.bytes)}; });

          if (includeContent) {
            // Request the file contents from the storage facility
            DataReadRequest2 readRequest;
            readRequest.mIds.reserve(ctx->subjects.size());
            std::transform(ctx->subjects.cbegin(), ctx->subjects.cend(), std::back_inserter(readRequest.mIds),
                           [](const EnumerateResult& subject) { return subject.mId; });
            readRequest.mTicket = *ticket;
            return clientStorageFacility->sendRequest(std::make_shared<std::string>(
                  Serialization::ToString(sign(readRequest))))
              .map([](std::string rawPage) {
                // Deserialize page
                return Serialization::FromString<DataPayloadPage>(std::move(rawPage));
              })
              .group_by([](const DataPayloadPage& page) { return page.mIndex; })
              .map([ctx, offset](const rxcpp::grouped_observable<uint32_t, DataPayloadPage>& groupedPages) {
                const auto index = groupedPages.get_key();
                auto ret = std::make_shared<RetrieveResult>();
                ret->mIndex = offset + index;
                ret->mMetadataDecrypted = ctx->subjects.at(index).mMetadata.decrypt(ctx->files.at(index).key);
                // Add observable for content
                ret->mContent = groupedPages
                  .op(RxIndexed<decltype(DataPayloadPage::mPageNumber)>())
                  .map([ctx, index](const std::pair<decltype(DataPayloadPage::mPageNumber), DataPayloadPage>& indexedPage) {
                    const auto& [ordinal, page] = indexedPage;
                    if (ordinal != page.mPageNumber) {
                      throw std::runtime_error("Received out of order page: expected " + std::to_string(ordinal) + " but got " + std::to_string(page.mPageNumber));
                    }
                    fileContext& file = ctx->files.at(index);
                    const EnumerateResult& entry = ctx->subjects.at(index);
                    std::string chunk = *page.decrypt(file.key, entry.mMetadata);
                    file.written += chunk.size();
                    if (file.written > entry.mFileSize) {
                      throw std::runtime_error("Received file larger than signaled file size");
                    }
                    return chunk;
                  })
                  .as_dynamic() // Reduce compiler memory usage
                  .op(RxBeforeCompletion([ctx, index] {
                    if (ctx->files.at(index).written < ctx->subjects.at(index).mFileSize) {
                      throw std::runtime_error("Received file smaller than signaled file size");
                    }
                  }));
                return ret;
              });
            } else {
              // Only decrypt metadata
              return rxcpp::observable<>::iterate(ctx->subjects)
                .zip(rxcpp::observable<>::iterate(ctx->files))
                .op(RxIndexed<uint32_t>())
                .map([ctx, offset](const std::pair<uint32_t, std::tuple<EnumerateResult, fileContext>>& entryData) {
                  const auto& [index, entryFile] = entryData;
                  const auto& [entry, file] = entryFile;
                  auto ret = std::make_shared<RetrieveResult>();
                  ret->mIndex = offset + index;
                  ret->mMetadataDecrypted = entry.mMetadata.decrypt(file.key);
                  return ret;
                });
            }
        });
    });
}

rxcpp::observable<std::vector<HistoryResult>>
CoreClient::getHistory2(SignedTicket2 ticket,
  const std::optional<std::vector<PolymorphicPseudonym>>& pps,
  const std::optional<std::vector<std::string>>& columns) {
  LOG(LOG_TAG, debug) << "getHistory";

  auto request = std::make_shared<DataHistoryRequest2>();
  request->mTicket = std::move(ticket);

  std::optional<Ticket2> unsignedTicket;
  FillHistoryRequestIndices<LocalPseudonyms, PolymorphicPseudonym>(
    request->mTicket, unsignedTicket, &Ticket2::mPseudonyms, pps, request->mPseudonyms, [](const LocalPseudonyms& lps, const PolymorphicPseudonym& pp) {return lps.mPolymorphic == pp; });
  FillHistoryRequestIndices<std::string, std::string>(
    request->mTicket, unsignedTicket, &Ticket2::mColumns, columns, request->mColumns, [](const std::string& ticketCol, const std::string& specifiedCol) {return ticketCol == specifiedCol; });

  return clientStorageFacility->sendRequest(std::make_shared<std::string>(Serialization::ToString(
    sign(*request))))
    .map([](std::string rawResponse) {
      auto response = Serialization::FromString<DataHistoryResponse2>(std::move(rawResponse));
      return response.mEntries;
    })
    .op(RxConcatenateVectors())
    .flat_map([this, request](std::shared_ptr<std::vector<DataHistoryEntry2>> entries) {
      const auto& ticket = request->mTicket.openWithoutCheckingSignature();
      std::vector<HistoryResult> results;
      results.reserve(entries->size());
      std::unordered_map<uint32_t, std::shared_ptr<LocalPseudonyms>> localPseuds;
      std::unordered_map<uint32_t, std::shared_ptr<LocalPseudonym>> agPseuds;
      std::transform(entries->cbegin(), entries->cend(), std::back_inserter(results), [this, &ticket, localPseuds, agPseuds](const DataHistoryEntry2& entry) mutable {
        auto ilp = localPseuds.find(entry.mPseudonymIndex);
        if (ilp == localPseuds.cend()) {
          auto emplaced = localPseuds.emplace(std::make_pair(entry.mPseudonymIndex, MakeSharedCopy(ticket.mPseudonyms[entry.mPseudonymIndex])));
          assert(emplaced.second);
          ilp = emplaced.first;
        }
        auto localPseudonyms = ilp->second;

        std::shared_ptr<LocalPseudonym> accessGroupPseudonym;
        if (localPseudonyms->mAccessGroup) {
          auto iag = agPseuds.find(entry.mPseudonymIndex);
          if (iag == agPseuds.cend()) {
            auto ag = localPseudonyms->mAccessGroup->decrypt(privateKeyPseudonyms);
            auto emplaced = agPseuds.emplace(std::make_pair(entry.mPseudonymIndex, MakeSharedCopy(ag)));
            assert(emplaced.second);
            iag = emplaced.first;
          }
          accessGroupPseudonym = iag->second;
        }

        return HistoryResult{
          DataCellResult{
            .mLocalPseudonyms = localPseudonyms,
            .mLocalPseudonymsIndex = entry.mPseudonymIndex,
            .mColumn = ticket.mColumns[entry.mColumnIndex],
            .mAccessGroupPseudonym = accessGroupPseudonym,
          },
          entry.mTimestamp,
          !entry.mId.empty() ? std::optional{entry.mId} : std::nullopt,
        };
        });
      return rxcpp::observable<>::just(results);
    });
}

CoreClient::TicketPseudonyms::TicketPseudonyms(const SignedTicket2& ticket, const ElgamalPrivateKey& privateKeyPseudonyms) {
  auto opened = ticket.openWithoutCheckingSignature();

  mPseudonyms.reserve(opened.mPseudonyms.size());
  if (!opened.mPseudonyms.empty()) {
    if (opened.mPseudonyms.front().mAccessGroup.has_value()) {
      mAgPseuds.emplace(std::vector<std::shared_ptr<LocalPseudonym>>());
      mAgPseuds->reserve(opened.mPseudonyms.size());
    }
  }

  for (const auto& p : opened.mPseudonyms) {
    mPseudonyms.push_back(std::make_shared<LocalPseudonyms>(p));

    if (p.mAccessGroup.has_value() != mAgPseuds.has_value()) {
      throw std::runtime_error("Inconsistent access group pseudonym presence in ticket");
    }

    if (mAgPseuds.has_value()) {
      mAgPseuds->push_back(std::make_shared<LocalPseudonym>(
        p.mAccessGroup->decrypt(privateKeyPseudonyms)));
    }
  }
}

std::shared_ptr<LocalPseudonym> CoreClient::TicketPseudonyms::getAccessGroupPseudonym(uint32_t index) const {
  if (mAgPseuds.has_value()) {
    return mAgPseuds->at(index);
  }
  return nullptr; // We don't have access group pseudonyms
}

/// Convert from DataEnumerationEntry2 to EnumerateResult.
/// Converts pseudonym indices to object references
std::vector<EnumerateResult> CoreClient::convertDataEnumerationEntries(
  const std::vector<DataEnumerationEntry2>& entries,
  const TicketPseudonyms& pseudonyms) const {

  std::vector<EnumerateResult> ress;
  ress.reserve(entries.size());
  for (const DataEnumerationEntry2& entry : entries) {
    EnumerateResult& r = ress.emplace_back();
    r.mId = entry.mId;
    r.mMetadata = entry.mMetadata;
    r.mPolymorphicKey = entry.mPolymorphicKey;
    r.mColumn = entry.mMetadata.getTag();
    r.mLocalPseudonymsIndex = entry.mPseudonymIndex;
    r.mFileSize = entry.mFileSize;
    r.mAccessGroupPseudonym = pseudonyms.getAccessGroupPseudonym(entry.mPseudonymIndex);
    r.mLocalPseudonyms = pseudonyms.getLocalPseudonyms(entry.mPseudonymIndex);
  }

  return ress;
}

}
