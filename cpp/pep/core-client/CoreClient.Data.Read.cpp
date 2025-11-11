#include <pep/core-client/CoreClient.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxConcatenateVectors.hpp>
#include <pep/async/RxFilterNullopt.hpp>
#include <pep/async/RxIndexed.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/storagefacility/DataPayloadPageStreamOrder.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>

#include <rxcpp/operators/rx-buffer_count.hpp>
#include <rxcpp/operators/rx-concat.hpp>
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

rxcpp::observable<std::vector<std::shared_ptr<EnumerateResult>>> CoreClient::enumerateData(const std::vector<std::string>&
                                                                           participantGroups,
                                                                       const std::vector<PolymorphicPseudonym>& pps,
                                                                       const std::vector<std::string>& columnGroups,
                                                                       const std::vector<std::string>& columns) {
  return accessManagerProxy
      ->requestTicket(ClientSideTicketRequest2{.mModes = {"read"},
                                               .mParticipantGroups = participantGroups,
                                               .mPolymorphicPseudonyms = pps,
                                               .mColumnGroups = columnGroups,
                                               .mColumns = columns,
                                               .mIncludeUserGroupPseudonyms = false})
      .flat_map([this](SignedTicket2 ticket) { return this->enumerateData(std::make_shared<SignedTicket2>(ticket)); });
}

rxcpp::observable<std::vector<std::shared_ptr<EnumerateResult>>> CoreClient::enumerateData(std::shared_ptr<SignedTicket2> ticket) {
  LOG(LOG_TAG, debug) << "enumerateData";

  auto pseudonyms = std::make_shared<TicketPseudonyms>(*ticket, privateKeyPseudonyms);
  auto enumRequest = std::make_shared<DataEnumerationRequest2>();
  enumRequest->mTicket = *ticket;
  return storageFacilityProxy->requestDataEnumeration(std::move(*enumRequest))
    .map([pseudonyms](DataEnumerationResponse2 response) {
      return ConvertDataEnumerationEntries(std::move(response.mEntries), *pseudonyms);
    });
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<EnumerateResult>>>
CoreClient::enumerateDataByIds(std::vector<std::string> ids, std::shared_ptr<SignedTicket2> ticket) {
  LOG(LOG_TAG, debug) << "enumerateDataByIds";

  auto pseudonyms = std::make_shared<TicketPseudonyms>(*ticket, privateKeyPseudonyms);

  return RxIterate(std::move(ids))
      .buffer(static_cast<int>(DATA_RETRIEVAL_BATCH_SIZE))
      .as_dynamic() // Reduce compiler memory usage
      //TODO shouldn't we be using shared_from_this()?
      .map([this, ticket, pseudonyms](std::vector<std::string> batch)
        -> rxcpp::observable<std::shared_ptr<EnumerateResult>> {
            auto entryCount = batch.size();
            return this->storageFacilityProxy->requestMetadataRead(
                MetadataReadRequest2{
                  .mTicket = *ticket,
                  .mIds = std::move(batch),
                })
                .map([](DataEnumerationResponse2 response) {
                  return std::move(response.mEntries);
                })
                .op(RxConcatenateVectors())
                .flat_map([entryCount, pseudonyms](const std::shared_ptr<std::vector<DataEnumerationEntry2>>& entries) {
                  if (entries->size() != entryCount) {
                    throw std::runtime_error("Storage facility return an unexpected number of entries");
                  }
                  return RxIterate(ConvertDataEnumerationEntries(std::move(*entries), *pseudonyms));
                });
          });
}

rxcpp::observable<rxcpp::observable<FileKey>>
CoreClient::getKeys(
  const rxcpp::observable<std::shared_ptr<EnumerateResult>>& subjects,
  std::shared_ptr<SignedTicket2> ticket) {

  return subjects
      .buffer(static_cast<int>(DATA_RETRIEVAL_BATCH_SIZE))
      .as_dynamic() // Reduce compiler memory usage
      .op(RxIndexed<std::uint32_t>())
      .map([this, ticket](std::pair<std::uint32_t, std::vector<std::shared_ptr<EnumerateResult>>> batchSubjects)
        -> rxcpp::observable<FileKey> {
        auto& [batchNum, subjects] = batchSubjects;
        auto fileOffset = batchNum * DATA_RETRIEVAL_BATCH_SIZE;
        auto keys = this->unblindAndDecryptKeys(subjects, ticket) // Get keys
          .op(RxConcatenateVectors());
        return keys
          .flat_map([fileOffset, subjects = MakeSharedCopy(std::move(subjects))](const std::shared_ptr<std::vector<AESKey>>& keys) {
            if (keys->size() != subjects->size()) {
              throw std::runtime_error("KeyResponse contains the wrong number of entries");
            }
            return rxcpp::observable<>::range(std::size_t{}, subjects->size() - 1)
              .map([fileOffset, subjects, keys](std::size_t index) {
                return FileKey{
                  .fileIndex = static_cast<std::uint32_t>(fileOffset + index),
                  .entry = std::move((*subjects)[index]),
                  .symmetricKey = std::move((*keys)[index].bytes),
                };
              });
          });
      });
}

rxcpp::observable<rxcpp::observable<RetrievePage>>
CoreClient::retrieveData(
  const rxcpp::observable<std::shared_ptr<EnumerateResult>>& subjects,
  std::shared_ptr<SignedTicket2> ticket) {
  return retrieveData(getKeys(subjects, ticket), ticket);
}

rxcpp::observable<rxcpp::observable<RetrievePage>>
CoreClient::retrieveData(
  const rxcpp::observable<rxcpp::observable<FileKey>>& batchedSubjects,
  std::shared_ptr<SignedTicket2> ticket) {
  LOG(LOG_TAG, debug) << "retrieveData";

  using namespace std::ranges;
  return batchedSubjects
      .map([this, ticket](const rxcpp::observable<FileKey>& batch) -> rxcpp::observable<RetrievePage> {
        return batch.op(RxToVector())
            .flat_map([this, ticket](const std::shared_ptr<std::vector<FileKey>>& batch) -> rxcpp::observable<RetrievePage> {
              struct fileContext {
                FileKey fileKey;
                std::uint64_t bytesWritten = 0;
              };
              struct batchContext {
                DataPayloadPageStreamOrder order;
                std::vector<fileContext> files;
              };

              auto ctx = std::make_shared<batchContext>();
              ctx->files = RangeToCollection<std::vector<fileContext>>(std::move(*batch));

              // Request the file contents from the storage facility
              auto pagesFromServer =
                  storageFacilityProxy->requestDataRead(DataReadRequest2{
                    .mTicket = *ticket,
                    .mIds = RangeToVector(ctx->files
                        | views::transform([](const fileContext& file) { return file.fileKey.entry->mId; })),
                  })
                  .map([](DataPayloadPage page) {
                    return std::optional{std::move(page)};
                  })
                  // Add nullopt sentinel to make sure we check if all files have been fully retrieved
                  .concat(rxcpp::observable<>::just(std::optional<DataPayloadPage>()))
                  .map([ctx](const std::optional<DataPayloadPage>& page) -> std::optional<RetrievePage> {
                    const auto index = page ? page->mIndex : ctx->files.size();
                    if (page && index >= ctx->files.size()) {
                      throw std::runtime_error(std::format("Received out-of-bounds file index: {} >= {}",
                          index, ctx->files.size()));
                    }

                    // Check previous file(s)
                    for (auto betweenIdx = ctx->order.latestFileIndex(); betweenIdx < index; ++betweenIdx) {
                      const fileContext& prevFileCtx = ctx->files[betweenIdx];
                      const EnumerateResult& prevEntry = *prevFileCtx.fileKey.entry;
                      if (prevFileCtx.bytesWritten < prevEntry.mFileSize) {
                        throw std::runtime_error(std::format(
                            "Received file {} smaller than signaled file size: {} < {}",
                            betweenIdx, prevFileCtx.bytesWritten, prevEntry.mFileSize));
                      }
                    }
                    // Return when we received the sentinel: we just wanted to check remaining files
                    if (!page) { return {}; }

                    ctx->order.check(*page);

                    fileContext& file = ctx->files[index];
                    const EnumerateResult& entry = *file.fileKey.entry;
                    RetrievePage retrievedPage{
                      .fileIndex = file.fileKey.fileIndex,
                      .entry = file.fileKey.entry,
                      .content = page->decrypt(file.fileKey.symmetricKey, entry.mMetadata),
                    };
                    // Omit empty pages
                    if (retrievedPage.content.empty()) { return {}; }
                    file.bytesWritten += retrievedPage.content.size();
                    if (file.bytesWritten > entry.mFileSize) {
                      throw std::runtime_error(std::format(
                          "Received file {} larger than signaled file size: {}+ > {}",
                          index, file.bytesWritten, entry.mFileSize));
                    }
                    retrievedPage.last = file.bytesWritten == entry.mFileSize;
                    return retrievedPage;
                  })
                  .op(RxFilterNullopt());

              auto emptyFiles = ctx->files
                  | views::filter([](const fileContext& file) {
                    return file.fileKey.entry->mFileSize == 0;
                  })
                  | views::transform([](const fileContext& file) {
                    return RetrievePage{
                      .fileIndex = file.fileKey.fileIndex,
                      .entry = file.fileKey.entry,
                      .content = {},
                      .last = true,
                    };
                  });

              return RxIterate(RangeToVector(emptyFiles))
                  .concat(std::move(pagesFromServer));
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

  return storageFacilityProxy->requestDataHistory(std::move(*request))
    .map([](const DataHistoryResponse2& response) {
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
std::vector<std::shared_ptr<EnumerateResult>> CoreClient::ConvertDataEnumerationEntries(
  std::vector<DataEnumerationEntry2>&& entries,
  const TicketPseudonyms& pseudonyms) {

  std::vector<std::shared_ptr<EnumerateResult>> ress;
  ress.reserve(entries.size());
  for (DataEnumerationEntry2& entry : entries) {
    EnumerateResult& r = *ress.emplace_back(std::make_shared<EnumerateResult>());
    r.mColumn = entry.mMetadata.getTag();

    r.mId = std::move(entry.mId);
    r.mMetadata = std::move(entry.mMetadata);
    r.mPolymorphicKey = entry.mPolymorphicKey;
    r.mLocalPseudonymsIndex = entry.mPseudonymIndex;
    r.mFileSize = entry.mFileSize;
    r.mAccessGroupPseudonym = pseudonyms.getAccessGroupPseudonym(entry.mPseudonymIndex);
    r.mLocalPseudonyms = pseudonyms.getLocalPseudonyms(entry.mPseudonymIndex);
  }

  return ress;
}

}
