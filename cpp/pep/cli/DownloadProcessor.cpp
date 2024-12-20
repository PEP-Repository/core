#include <pep/cli/DownloadProcessor.hpp>
#include <pep/async/RxUtils.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

namespace {

const std::string LOG_TAG("DownloadProcessor");

rxcpp::observable<std::shared_ptr<std::vector<std::optional<Timestamp>>>> GetPayloadEntryBlindingTimestamps(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const std::vector<EnumerateResult>& metaEntries) {
  // Collect (IDs of) entries containing original payload
  std::vector<std::string> payloadIds;
  auto metaIndices = std::make_shared<std::vector<size_t>>(); // positions correspond with payload(Id)s; values are indices in "metaEntries"
  for (size_t i = 0U; i < metaEntries.size(); ++i) {
    const auto& metadata = metaEntries[i].mMetadata;
    if (metadata.getOriginalPayloadEntryId().has_value()) {
      payloadIds.emplace_back(*metadata.getOriginalPayloadEntryId());
      metaIndices->emplace_back(i);
    }
  }

  // Don't perform network calls if there's nothing to retrieve
  if (payloadIds.empty()) {
    auto result = std::make_shared<std::vector<std::optional<Timestamp>>>();
    result->resize(metaEntries.size());
    return rxcpp::observable<>::just(result);
  }

  // If we're here, the "metaEntries" contains at least one entry that represents a metadata-only update, so we
  // retrieve original payload entries from the server and extract their timestamps.
  return client->retrieveData2(client->getMetadata(payloadIds, ticket), ticket, false)
    .reduce( // Associate payload entry indices with timestamps
      std::make_shared<std::map<size_t, Timestamp>>(),
      [](std::shared_ptr<std::map<size_t, Timestamp>> timestamps, std::shared_ptr<RetrieveResult> payloadEntry) {
        if (payloadEntry->mMetadataDecrypted.getOriginalPayloadEntryId().has_value()) {
          throw std::runtime_error("Received a metadata-only update entry from server, but expected a payload-bearing entry");
        }
        auto emplaced = timestamps->emplace(payloadEntry->mIndex, payloadEntry->mMetadataDecrypted.getBlindingTimestamp()).second;
        if (!emplaced) {
          throw std::runtime_error("Received duplicate payload-bearing entry indices from server");
        }
        return timestamps;
      }
    )
    .map([metaCount = metaEntries.size(), metaIndices](std::shared_ptr<std::map<size_t, Timestamp>> payloadTimestamps) { // Match timestamps to items in "metaEntries" vector
        if (payloadTimestamps->size() != metaIndices->size()) {
          throw std::runtime_error("Received an unexpected number of RetrieveResults from payload entry retrieval");
        }
        auto result = std::make_shared<std::vector<std::optional<Timestamp>>>();
        result->resize(metaCount);
        for (const auto& payloadTimestamp : *payloadTimestamps) {
          auto payloadIndex = payloadTimestamp.first;
          assert(payloadIndex < metaIndices->size());
          auto metaIndex = (*metaIndices)[payloadIndex];
          (*result)[metaIndex] = payloadTimestamp.second;
        }
        return result;
      });
}

}


void DownloadProcessor::fail(const std::string& message) {
  throw std::runtime_error(message);
}

rxcpp::observable<FakeVoid> DownloadProcessor::update(std::shared_ptr<CoreClient> client, const DownloadDirectory::PullOptions& options, const Progress::OnCreation& onCreateProgress) {
  struct Context {
    DownloadDirectory::ContentSpecification content;
    std::shared_ptr<IndexedTicket2> ticket;
    std::vector<EnumerateResult> downloads;
    std::vector<RecordDescriptor> descriptors;
    std::vector<std::shared_ptr<DownloadDirectory::RecordStorageStream>> streams;
    std::shared_ptr<CoreClient> client;
    DownloadDirectory::PullOptions options;
  };

  auto ctx = std::make_shared<Context>();
  ctx->content = mDestination->getSpecification().content;
  ctx->client = client;
  ctx->options = options;

  auto progress = Progress::Create(4U, onCreateProgress);

  // First request a ticket
  requestTicket2Opts tOpts;
  tOpts.pps = ctx->content.pps;
  tOpts.columns = ctx->content.columns;
  tOpts.columnGroups = ctx->content.columnGroups;
  tOpts.participantGroups = ctx->content.groups;
  tOpts.modes = { "read" };
  tOpts.includeAccessGroupPseudonyms = true;
  progress->advance("Requesting ticket");
  return ctx->client->requestTicket2(tOpts)
    .flat_map([ctx, progress](IndexedTicket2 ticket) {
    ctx->ticket = std::make_shared<IndexedTicket2>(std::move(ticket));

    // Now list matching files
    progress->advance("Listing files");
    return ctx->client->enumerateData2(ctx->ticket->getTicket());
  })
    .op(RxConcatenateVectors()) // Aggregate entries into a single vector<EnumerateResult>
    .flat_map([ctx](std::shared_ptr<std::vector<EnumerateResult>> metas) {
    return GetPayloadEntryBlindingTimestamps(ctx->client, ctx->ticket->getTicket(), *metas) // Get (blinding) timestamps when payloads for these EnumerateResults were originally uploaded
      .map([metas](std::shared_ptr<std::vector<std::optional<Timestamp>>> payloadTimestamps) { // Convert vector<>s to unordered_map<> for speedy lookup
      auto mapped = std::make_shared<std::unordered_map<RecordDescriptor, EnumerateResult>>();
      mapped->reserve(metas->size());
      for (size_t i = 0U; i < metas->size(); ++i) {
        const EnumerateResult& entry = (*metas)[i];
        const std::optional<Timestamp>& payloadTimestamp = (*payloadTimestamps)[i];
        ParticipantIdentifier id(
          entry.mLocalPseudonyms->mPolymorphic,
          *entry.mAccessGroupPseudonym);
        [[maybe_unused]] auto emplaced = mapped->emplace(
          RecordDescriptor(id, entry.mColumn, entry.mMetadata.getBlindingTimestamp(), entry.mMetadata.extra(), payloadTimestamp),
          entry);
        assert(emplaced.second);
      }
      return mapped;
        });
      })
    .flat_map([self = shared_from_this(), ctx, progress](std::shared_ptr<std::unordered_map<RecordDescriptor, EnumerateResult>> downloads) {
    progress->advance("Preparing local data");
    for (const auto& existing : self->mDestination->list()) {
      auto position = std::find_if(downloads->begin(), downloads->end(), [&existing](const auto& pair) {
        const RecordDescriptor& candidate = pair.first;
        return candidate.getParticipant().getLocalPseudonym() == existing.getParticipant().getLocalPseudonym()
          && candidate.getColumn() == existing.getColumn()
          && candidate.getPayloadBlindingTimestamp() == existing.getPayloadBlindingTimestamp();
        });
      if (position == downloads->cend()) {
        // Payload is not in the server's current data set: it has either been removed from the server,
        // or the payload will be updated to a newer version (i.e. same participant and column, but different timestamp)
        if (!self->mDestination->remove(existing)) {
          if (ctx->options.assumePristine) {
            auto update = std::find_if(downloads->cbegin(), downloads->cend(), [&existing](const std::pair<const RecordDescriptor, EnumerateResult>& enumerated) {
              return *enumerated.second.mAccessGroupPseudonym == existing.getParticipant().getLocalPseudonym()
                && enumerated.second.mColumn == existing.getColumn();
            });
            if (update == downloads->cend()) {
              // Data should have been removed from the local copy, but it wasn't there
              LOG("update", pep::warning) << "Could not remove data that was assumed to be pristine: participant " << existing.getParticipant().getLocalPseudonym().text()
                << "; column " << existing.getColumn()
                << "; blinding timestamp " << existing.getBlindingTimestamp().getTime();
            }
          }
        }
      }
      else if (ctx->options.assumePristine || self->mDestination->hasPristineData(existing)) {
        // We already have the payload: don't download
        if (existing.getBlindingTimestamp() != position->first.getBlindingTimestamp()) {
          // Server has different metadata than our download directory: apply metadata-only update to the payload that we already have
          if (!self->mDestination->update(existing, position->first)) {
            if (ctx->options.assumePristine) {
              // Data file should have been renamed in the local copy, but it wasn't there
              LOG("update", pep::warning) << "Could not rename data file that was assumed to be pristine: participant " << existing.getParticipant().getLocalPseudonym().text()
                << "; column " << existing.getColumn()
                << "; blinding timestamp " << existing.getBlindingTimestamp().getTime();
            }
          }
        }
        downloads->erase(position);
      }
      else {
        // Our copy is not pristine: payload will be downloaded
        self->mDestination->remove(existing);
      }
    }

    // (Re-)initialize the fields that will be updated
    ctx->descriptors.reserve(downloads->size());
    ctx->downloads.reserve(downloads->size());
    for (const auto& entry : *downloads) {
      ctx->descriptors.emplace_back(entry.first);
      ctx->downloads.emplace_back(entry.second);
    }
    ctx->streams.resize(ctx->downloads.size());

    // Retrieve data for fields that we're updating
    progress->advance("Retrieving from server");
    auto retrieveProgress = Progress::Create(ctx->downloads.size(), progress->push());
    return ctx->client->retrieveData2(rxcpp::observable<>::iterate(ctx->downloads), ctx->ticket->getTicket(), true)
      .flat_map([self, ctx, retrieveProgress](std::shared_ptr<RetrieveResult> result) { // Process data chunk
      auto stream = ctx->streams.at(result->mIndex);
      if (stream == nullptr) {
        // Receiving the first part of the record: open stream now
        const RecordDescriptor& descriptor = ctx->descriptors.at(result->mIndex);
        stream = self->openStorageStream(descriptor, ctx->downloads.at(result->mIndex).mFileSize, *retrieveProgress);
        ctx->streams[result->mIndex] = stream;
      }

      assert(result->mContent);

      return result->mContent
        ->map([self, stream](const std::string& chunk) {
          if (stream->isCommitted() && chunk.empty()) { //stream->write will throw an error if it has already been commited, but we want to allow an empty chunk
            LOG(LOG_TAG, warning) << "Trying to write empty chunk to record that has already been committed.";
          }
          else {
            stream->write(chunk, self->mGlobalConfig);
          }
          return FakeVoid();
        });
      })
      .op(RxBeforeCompletion([self, ctx, retrieveProgress]() {
        for (size_t i = 0U; i < ctx->streams.size(); ++i) {
          if (ctx->streams[i] == nullptr) {
            // We've received no content (chunk) for this download, so it must be an empty file: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2337
            ctx->streams[i] = self->openStorageStream(ctx->descriptors.at(i), 0U, *retrieveProgress);
            ctx->streams[i]->commit(self->mGlobalConfig);
          }
        }
        retrieveProgress->advanceToCompletion();
        }));
    })
    .op(RxBeforeCompletion([progress]() {progress->advanceToCompletion(); }))
    .op(RxInstead(FakeVoid())); // Return a single FakeVoid for the entire operation

}

std::shared_ptr<DownloadDirectory::RecordStorageStream> DownloadProcessor::openStorageStream(const RecordDescriptor& descriptor, size_t fileSize, Progress& progress) {
  bool pseudonymisationRequired{ false };
  bool archiveExtractionRequired{ false };
  if (auto columnSpecification = mGlobalConfig->getColumnSpecification(descriptor.getColumn())) {
    if (columnSpecification->getAssociatedShortPseudonymColumn().has_value()) {
      pseudonymisationRequired = true;
    }
    archiveExtractionRequired = columnSpecification->getRequiresDirectory();
  }

  auto result = mDestination->create(descriptor, pseudonymisationRequired, archiveExtractionRequired, fileSize);
  progress.advance(result->getRelativePath().string());
  return result;
}

}
