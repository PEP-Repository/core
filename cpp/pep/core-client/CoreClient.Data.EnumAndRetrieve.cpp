// The enumerateAndRetrieveData2 function alone took around 30 s and 3.5 GB RAM to compile on gcc,
// so it had to be split from CoreClient.Data.cpp

#include <pep/core-client/CoreClient.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/storagefacility/DataPayloadPage.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/utils/Shared.hpp>

#include <pep/storagefacility/StorageFacilitySerializers.hpp>

#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

using namespace pep;

namespace {

const std::string LOG_TAG("CoreClient.Data");

template<typename TResponse>
rxcpp::observable<TResponse> BatchedRetrieve(
  const std::vector<std::string>& ids,
  const std::function<void(size_t, TResponse&)>& updateIndex,
  const std::function<rxcpp::observable<TResponse>(const std::vector<std::string>&)>& retrieveBatch) {

  // Degenerate case: all IDs can be retrieved in a single batch
  if (ids.size() <= CoreClient::DATA_RETRIEVAL_BATCH_SIZE) {
    return retrieveBatch(ids);
  }

  // Split IDs over batches
  auto batches = std::make_shared<std::vector<std::vector<std::string>>>();
  batches->reserve(ids.size() / CoreClient::DATA_RETRIEVAL_BATCH_SIZE + 1);
  for (size_t offset = 0U; offset < ids.size(); offset += CoreClient::DATA_RETRIEVAL_BATCH_SIZE) {
    size_t batchSize = std::min(CoreClient::DATA_RETRIEVAL_BATCH_SIZE, ids.size() - offset);
    auto& batch = batches->emplace_back();
    batch.reserve(batchSize);
    std::copy(ids.cbegin() + static_cast<ptrdiff_t>(offset),
              ids.cbegin() + static_cast<ptrdiff_t>(offset + batchSize),
              std::back_inserter(batch));
  }

  /* Documentation on e.g. http://reactivex.io/documentation/operators/range.html says
   * that the RX range operator accepts start and **length** parameters, but the
   * rxcpp implementation seems to accept a start and **last** value.
   * Since our range is 0-based, the last value is the number of batches minus 1.
  */
  return rxcpp::observable<>::range(ptrdiff_t{0}, static_cast<ptrdiff_t>(batches->size() - 1)) // Iterate over batch(indic)es
    .flat_map([batches, retrieveBatch, updateIndex](ptrdiff_t batchIndex) { // Process each batch individually
      const auto& batch = (*batches)[static_cast<size_t>(batchIndex)];
      size_t offset = static_cast<size_t>(batchIndex) * CoreClient::DATA_RETRIEVAL_BATCH_SIZE;
      return retrieveBatch(batch) // Retrieve this batch
        .tap([offset, updateIndex](TResponse response) {
          updateIndex(offset, response);
        }); // Update response indices to have them match indices of the "ids" parameter
    });
}

} // namespace


rxcpp::observable<EnumerateAndRetrieveResult>
CoreClient::enumerateAndRetrieveData2(const enumerateAndRetrieveData2Opts& opts) {
  LOG(LOG_TAG, debug) << "enumerateAndRetrieveData";

  using Pages = std::vector<std::shared_ptr<DataPayloadPage>>;
  using IndexedPages = std::unordered_map<uint32_t, std::shared_ptr<Pages>>;
  struct Context {
    bool includeData{};
    uint64_t dataSizeLimit{};
    std::shared_ptr<requestTicket2Opts> requestTicketOpts;
    std::optional<rxcpp::subscriber<EnumerateAndRetrieveResult>> subscriber;
    std::shared_ptr<SignedTicket2> signedTicket;
    std::shared_ptr<Ticket2> ticket;
    std::unique_ptr<TicketPseudonyms> pseudonyms;
    std::vector<std::string> keys;
    std::shared_ptr<IndexedPages> pages;
  };
  auto ctx = std::make_shared<Context>();
  ctx->includeData = opts.includeData;
  ctx->dataSizeLimit = opts.dataSizeLimit;

  ctx->requestTicketOpts = std::make_shared<requestTicket2Opts>();
  ctx->requestTicketOpts->modes = {opts.includeData ? "read" : "read-meta"};
  ctx->requestTicketOpts->participantGroups = opts.groups;
  ctx->requestTicketOpts->pps = opts.pps;
  ctx->requestTicketOpts->columnGroups = opts.columnGroups;
  ctx->requestTicketOpts->columns = opts.columns;
  ctx->requestTicketOpts->ticket = opts.ticket;
  ctx->requestTicketOpts->forceTicket = opts.forceTicket;
  ctx->requestTicketOpts->includeAccessGroupPseudonyms = opts.includeAccessGroupPseudonyms;

  /* Instead of using a simple sequence of RX operations, we store the (external) subscriber and
   * emit items to that subscriber directly. This makes the code much faster than when we have
   * RX's various operators process items, presumably because RX
   * - creates so many object copies: see e.g. https://flames-of-code.netlify.app/blog/rxcpp-copy-operator/
   * - keeps lots of stuff in memory when using e.g. concat_map: see
   *   https://gitlab.pep.cs.ru.nl/pep/core/-/blob/ad9ba4669d9bf562c67b0645d405ef602ddf417a/cpp/pep/castor/CastorClient.cpp#L106
   */
  return CreateObservable<EnumerateAndRetrieveResult>(
    [this, ctx](rxcpp::subscriber<EnumerateAndRetrieveResult> subscriber) {
      ctx->subscriber = subscriber;
      this->requestTicket2(*ctx->requestTicketOpts) // Get (indexed) ticket
          // .op(RxGetOne("ticket")) // Doesn't compile: see https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/1690#note_29119
        .flat_map([this, ctx](const IndexedTicket2& indexedTicket) {
          ctx->signedTicket = indexedTicket.getTicket();
          ctx->ticket = MakeSharedCopy(ctx->signedTicket->openWithoutCheckingSignature());
          ctx->pseudonyms = std::make_unique<TicketPseudonyms>(*ctx->signedTicket, privateKeyPseudonyms);
          DataEnumerationRequest2 enumRequest;
          enumRequest.mTicket = *ctx->signedTicket;

          if (ctx->requestTicketOpts->ticket != nullptr) {
            std::unordered_set<uint32_t> pseudIdxs;
            std::unordered_set<uint32_t> colIdxs;
            for (const auto& cg: ctx->requestTicketOpts->columnGroups) {
              for (uint32_t i: indexedTicket.getColumnGroupMapping().at(cg).mIndices) {
                colIdxs.insert(i);
              }
            }
            for (const auto& g: ctx->requestTicketOpts->participantGroups) {
              for (uint32_t i: indexedTicket.getParticipantGroupMapping().at(g).mIndices) {
                pseudIdxs.insert(i);
              }
            }
            if (!ctx->requestTicketOpts->pps.empty()) {
              auto ticketPseuds = indexedTicket.getPolymorphicPseudonyms();
              std::unordered_map<pep::PolymorphicPseudonym, uint32_t> lut;
              lut.reserve(ticketPseuds.size());
              for (size_t i = 0; i < ticketPseuds.size(); i++) {
                lut[ticketPseuds[i]] = static_cast<uint32_t>(i);
              }

              pseudIdxs.reserve(pseudIdxs.size() + ctx->requestTicketOpts->pps.size());
              for (const auto& pp: ctx->requestTicketOpts->pps) {
                pseudIdxs.insert(lut.at(pp));
              }
            }
            if (!ctx->requestTicketOpts->columns.empty()) {
              auto ticketCols = indexedTicket.getColumns();
              std::unordered_map<std::string, uint32_t> lut;
              lut.reserve(ticketCols.size());
              for (size_t i = 0; i < ticketCols.size(); i++) {
                lut[ticketCols[i]] = static_cast<uint32_t>(i);
              }

              colIdxs.reserve(colIdxs.size() + ctx->requestTicketOpts->columns.size());
              for (const auto& col: ctx->requestTicketOpts->columns) {
                colIdxs.insert(lut.at(col));
              }
            }
            enumRequest.mPseudonyms = IndexList(std::vector<uint32_t>(
              pseudIdxs.begin(), pseudIdxs.end()));
            enumRequest.mColumns = IndexList(std::vector<uint32_t>(
              colIdxs.begin(), colIdxs.end()));
          }

          return clientStorageFacility->sendRequest(std::make_shared<std::string>(Serialization::ToString(
              sign(enumRequest))))
            .reduce(
              std::make_shared<std::vector<DataEnumerationEntry2>>(),
              [ctx](std::shared_ptr<std::vector<DataEnumerationEntry2>> entriesWithData, std::string rawResponse) {
                auto response = Serialization::FromString<DataEnumerationResponse2>(std::move(rawResponse));
                for (auto& entry: response.mEntries) {
                  if (ctx->includeData && (ctx->dataSizeLimit == 0U || entry.mFileSize <= ctx->dataSizeLimit)) {
                    // This entry will include data: save it for data retrieval
                    entriesWithData->emplace_back(std::move(entry));
                  } else { // This entry won't include data: emit it now
                    EnumerateAndRetrieveResult res;
                    res.mDataSet = false;
                    res.mId = entry.mId;
                    res.mMetadata = entry.mMetadata;
                    res.mColumn = entry.mMetadata.getTag();
                    res.mLocalPseudonymsIndex = entry.mPseudonymIndex;
                    res.mLocalPseudonyms = ctx->pseudonyms->getLocalPseudonyms(entry.mPseudonymIndex);
                    res.mAccessGroupPseudonym = ctx->pseudonyms->getAccessGroupPseudonym(entry.mPseudonymIndex);
                    ctx->subscriber->on_next(std::move(res));
                  }
                }
                return entriesWithData;
              })
            .as_dynamic() // Reduce compiler memory usage
            .flat_map([this, ctx](
              std::shared_ptr<std::vector<DataEnumerationEntry2>> entries) -> rxcpp::observable<FakeVoid> { // Retrieve data for entries that need it
              // Degenerate case: return early if we don't have to retrieve any data
              if (entries->empty()) {
                return rxcpp::observable<>::empty<FakeVoid>();
              }

              // Create an observable that'll produce AES keys
              rxcpp::observable<FakeVoid> getKeys = this->unblindAndDecryptKeys(
                  convertDataEnumerationEntries(*entries, *ctx->pseudonyms), ctx->signedTicket)
                .map([ctx, entries](const std::vector<AESKey>& keys) {
                  if (keys.size() != entries->size()) {
                    throw std::runtime_error("Received unexpected number of plaintext keys");
                  }
                  assert(ctx->keys.empty());
                  ctx->keys.reserve(keys.size());
                  std::transform(keys.cbegin(), keys.cend(), std::back_inserter(ctx->keys),
                                 [](const AESKey& key) { return key.bytes; });
                  return FakeVoid();
                });

              // Create an observable that'll retrieve (encrypted) pages from Storage Facility
              std::vector<std::string> ids;
              ids.reserve(entries->size());
              std::transform(entries->cbegin(), entries->cend(), std::back_inserter(ids),
                             [](const DataEnumerationEntry2& entry) { return entry.mId; });
              rxcpp::observable<FakeVoid> getPages = BatchedRetrieve<std::shared_ptr<DataPayloadPage>>(
                ids,
                [](size_t offset,
                   std::shared_ptr<DataPayloadPage>& page) {
                  page->mIndex += static_cast<uint32_t>(
                    offset);
                },
                [this, ticket = ctx->signedTicket](
                  const std::vector<std::string>& ids) {
                  DataReadRequest2 readRequest;
                  readRequest.mIds = ids;
                  readRequest.mTicket = *ticket;
                  return clientStorageFacility->sendRequest(
                      std::make_shared<std::string>(
                        Serialization::ToString(
                          SignedDataReadRequest2(
                            readRequest,
                            certificateChain,
                            privateKey))))
                    .map([](
                      std::string rawPage) {
                      return MakeSharedCopy(
                        Serialization::FromString<DataPayloadPage>(
                          std::move(
                            rawPage)));
                    });
                })
                .op(RxGroupToVectors([](std::shared_ptr<DataPayloadPage> page) { return page->mIndex; }))
                .map([ctx](std::shared_ptr<IndexedPages> pages) {
                  assert(ctx->pages == nullptr);
                  ctx->pages = pages;
                  return FakeVoid();
                });

              return rxcpp::observable<>::just(getKeys).concat(
                  rxcpp::observable<>::just(getPages)) // Combine AES key retrieval and encrypted page retrieval
                .flat_map([](
                  rxcpp::observable<FakeVoid> job) { return job; }) // Retrieve AES keys and encrypted pages *concurrently* (because of *flat*_map)
                .as_dynamic() // Reduce compiler memory usage
                .op(RxBeforeCompletion(
                  [ctx, entries]() { // When both AES key retrieval and encrypted page retrieval have been completed...
                    assert(entries->size() == ctx->keys.size());

                    // Iterate over DataEnumerationResponse2 entries that we've retrieved data for
                    for (size_t i = 0U; i < entries->size(); ++i) {
                      const auto& entry = (*entries)[i];
                      const auto& key = ctx->keys[i];

                      // Convert Storage Facility's DataEnumerationResponse2 to CoreClient's EnumerateAndRetrieveResult
                      EnumerateAndRetrieveResult res;
                      res.mDataSet = true;
                      res.mId = entry.mId;
                      res.mMetadataDecrypted = entry.mMetadata.decrypt(key);
                      res.mColumn = entry.mMetadata.getTag();
                      res.mLocalPseudonymsIndex = entry.mPseudonymIndex;
                      res.mLocalPseudonyms = ctx->pseudonyms->getLocalPseudonyms(entry.mPseudonymIndex);
                      res.mAccessGroupPseudonym = ctx->pseudonyms->getAccessGroupPseudonym(entry.mPseudonymIndex);

                      // Fill the entry's data with the pages we retrieved for it
                      auto ipage = ctx->pages->find(static_cast<uint32_t>(i));
                      if (ipage != ctx->pages->cend()) {
                        auto& pages = *ipage->second;
                        std::stringstream buffer;
                        for (size_t i = 0U; i < pages.size(); ++i) {
                          assert(pages[i]->mPageNumber == i);
                          auto chunk = pages[i]->decrypt(key, entry.mMetadata);
                          buffer << *chunk;
                        }
                        res.mData = std::move(buffer).str();
                      }

                      // Verify consistency
                      if (res.mData.size() != entry.mFileSize) {
                        std::ostringstream message;
                        message << "Received " << res.mData.size()
                                << " bytes of data for a storage facility entry that was supposed to have "
                                << entry.mFileSize << " bytes";
                        throw std::runtime_error(message.str());
                      }

                      // Emit the item to the subscriber
                      ctx->subscriber->on_next(std::move(res));
                    }
                  }));
            });
        })
        .subscribe(
          [](FakeVoid) {/* ignore: subscriber.on_next is invoked during RX pipeline processing */},
          [ctx](std::exception_ptr error) {
            ctx->subscriber->on_error(error);
            ctx->subscriber = std::nullopt;
          },
          [ctx]() {
            ctx->subscriber->on_completed();
            ctx->subscriber = std::nullopt;
          }
        );
    });
}
