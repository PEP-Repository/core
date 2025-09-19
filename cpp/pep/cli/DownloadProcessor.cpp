#include <pep/cli/DownloadProcessor.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/async/RxDrain.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/utils/VectorOfVectors.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

using namespace pep;
using namespace pep::cli;

namespace {

const std::string LOG_TAG("DownloadProcessor");

rxcpp::observable<std::shared_ptr<std::vector<std::optional<Timestamp>>>> GetPayloadEntryBlindingTimestamps(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const VectorOfVectors<EnumerateResult>& metaEntries) {
  // Collect (IDs of) entries containing original payload
  std::vector<std::string> payloadIds;
  auto metaIndices = std::make_shared<std::vector<size_t>>(); // positions correspond with payload(Id)s; values are indices in "metaEntries"
  auto m = metaEntries.begin();
  for (size_t i = 0U; i < metaEntries.size(); ++i, ++m) {
    assert(m != metaEntries.end());
    const auto& metadata = m->mMetadata;
    if (metadata.getOriginalPayloadEntryId().has_value()) {
      payloadIds.emplace_back(*metadata.getOriginalPayloadEntryId());
      metaIndices->emplace_back(i);
    }
  }
  assert(m == metaEntries.end());

  // Don't perform network calls if there's nothing to retrieve
  if (payloadIds.empty()) {
    auto result = std::make_shared<std::vector<std::optional<Timestamp>>>();
    result->resize(metaEntries.size());
    return rxcpp::observable<>::just(result);
  }

  // If we're here, the "metaEntries" contains at least one entry that represents a metadata-only update, so we
  // retrieve original payload entries from the server and extract their timestamps.
  return client->getMetadata(payloadIds, ticket)
    .map([](const EnumerateResult& payloadEntry) {
    if (payloadEntry.mMetadata.getOriginalPayloadEntryId().has_value()) {
      throw std::runtime_error("Received a metadata-only update entry from server, but expected a payload-bearing entry");
    }
    return payloadEntry.mMetadata.getBlindingTimestamp();
      })
    .op(RxToVector())
    .map([metaCount = metaEntries.size(), metaIndices](std::shared_ptr<std::vector<Timestamp>> payloadTimestamps) { // Match timestamps to items in "metaEntries" vector
        if (payloadTimestamps->size() != metaIndices->size()) {
          throw std::runtime_error("Received an unexpected number of RetrieveResults from payload entry retrieval");
        }
        auto result = std::make_shared<std::vector<std::optional<Timestamp>>>();
        result->resize(metaCount);
        for (size_t i = 0U; i < payloadTimestamps->size(); ++i) {
          auto metaIndex = (*metaIndices)[i];
          (*result)[metaIndex] = (*payloadTimestamps)[i];
        }
        return result;
      });
}

}


void DownloadProcessor::fail(const std::string& message) {
  throw std::runtime_error(message);
}

struct DownloadProcessor::Context {
  DownloadDirectory::ContentSpecification content;
  std::shared_ptr<IndexedTicket2> ticket;
  std::vector<uint64_t> sizes;
  std::vector<std::shared_ptr<RecordDescriptor>> descriptors;
  std::vector<std::shared_ptr<DownloadDirectory::RecordStorageStream>> streams;
  std::shared_ptr<CoreClient> client;
  DownloadDirectory::PullOptions options;
};

rxcpp::observable<FakeVoid> DownloadProcessor::update(std::shared_ptr<CoreClient> client, const DownloadDirectory::PullOptions& options, const Progress::OnCreation& onCreateProgress) {
  auto ctx = std::make_shared<Context>();
  ctx->content = mDestination->getSpecification().content;
  ctx->client = client;
  ctx->options = options;

  auto progress = Progress::Create(5U, onCreateProgress);
  auto self = SharedFrom(*this);

  return this->requestTicket(progress, ctx)
    .flat_map([self, progress, ctx](std::shared_ptr<IndexedTicket2> ticket) { return self->listFiles(progress, ctx); })
    .flat_map([self, progress, ctx](std::shared_ptr<VectorOfVectors<EnumerateResult>> metas) { return self->locateFileContents(progress, ctx, metas); })
    .tap([self, progress, ctx](std::shared_ptr<std::unordered_map<RecordDescriptor, EnumerateResult>> downloads) { self->prepareLocalData(progress, downloads, ctx->options.assumePristine); })
    .flat_map([self, progress, ctx](std::shared_ptr<std::unordered_map<RecordDescriptor, EnumerateResult>> downloads) {return self->retrieveFromServer(progress, ctx, downloads); })
    .op(RxBeforeCompletion([progress]() {progress->advanceToCompletion(); }))
    .op(RxInstead(FakeVoid())); // Return a single FakeVoid for the entire operation
}

rxcpp::observable<std::shared_ptr<IndexedTicket2>> DownloadProcessor::requestTicket(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx) {
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
    .map([ctx](IndexedTicket2 ticket) {
    ctx->ticket = std::make_shared<IndexedTicket2>(std::move(ticket));
    return ctx->ticket;
      });
}

rxcpp::observable<std::shared_ptr<VectorOfVectors<EnumerateResult>>> DownloadProcessor::listFiles(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx) {
  progress->advance("Listing files");
  return ctx->client->enumerateData2(ctx->ticket->getTicket())
    .op(RxToVectorOfVectors());
}

rxcpp::observable<std::shared_ptr<std::unordered_map<RecordDescriptor, EnumerateResult>>> DownloadProcessor::locateFileContents(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx, std::shared_ptr<VectorOfVectors<EnumerateResult>> metas) {
  progress->advance("Locating file contents");
  return GetPayloadEntryBlindingTimestamps(ctx->client, ctx->ticket->getTicket(), *metas) // Get (blinding) timestamps when payloads for these EnumerateResults were originally uploaded
    .map([metas](std::shared_ptr<std::vector<std::optional<Timestamp>>> payloadTimestamps) { // Convert vector<>s to unordered_map<> for speedy lookup
    auto mapped = std::make_shared<std::unordered_map<RecordDescriptor, EnumerateResult>>();
    mapped->reserve(metas->size());

    assert(metas->size() == payloadTimestamps->size());
    auto m = metas->begin(), mend = metas->end();
    auto t = payloadTimestamps->begin();

    while (m != mend) {
      assert(t != payloadTimestamps->end());
      const EnumerateResult& entry = *m;
      const std::optional<Timestamp>& payloadTimestamp = *t;

      ParticipantIdentifier id(
        entry.mLocalPseudonyms->mPolymorphic,
        *entry.mAccessGroupPseudonym);
      [[maybe_unused]] auto emplaced = mapped->emplace(
        RecordDescriptor(id, entry.mColumn, entry.mMetadata.getBlindingTimestamp(), entry.mMetadata.extra(), payloadTimestamp),
        entry);
      assert(emplaced.second);

      ++m;
      ++t;
    }

    assert(m == mend);
    assert(t == payloadTimestamps->end());

    metas->clear(); // RX keeps the "metas" shared_ptr alive for longer than we need it, so we release memory by getting rid of the EnumerateResult instances
    return mapped;
      });
}

void DownloadProcessor::prepareLocalData(std::shared_ptr<Progress> progress, std::shared_ptr<std::unordered_map<RecordDescriptor, EnumerateResult>> downloads, bool assumePristine) {
  progress->advance("Preparing local data");
  for (const auto& existing : mDestination->list()) {
    auto position = std::find_if(downloads->begin(), downloads->end(), [&existing](const auto& pair) {
      const RecordDescriptor& candidate = pair.first;
      return candidate.getParticipant().getLocalPseudonym() == existing.getParticipant().getLocalPseudonym()
        && candidate.getColumn() == existing.getColumn()
        && candidate.getPayloadBlindingTimestamp() == existing.getPayloadBlindingTimestamp();
      });
    if (position == downloads->cend()) {
      // Payload is not in the server's current data set: it has either been removed from the server,
      // or the payload will be updated to a newer version (i.e. same participant and column, but different timestamp)
      if (!mDestination->remove(existing)) {
        if (assumePristine) {
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
    } else if (assumePristine || mDestination->hasPristineData(existing)) {
      // We already have the payload: don't download
      if (existing.getBlindingTimestamp() != position->first.getBlindingTimestamp()) {
        // Server has different metadata than our download directory: apply metadata-only update to the payload that we already have
        if (!mDestination->update(existing, position->first)) {
          if (assumePristine) {
            // Data file should have been renamed in the local copy, but it wasn't there
            LOG("update", pep::warning) << "Could not rename data file that was assumed to be pristine: participant " << existing.getParticipant().getLocalPseudonym().text()
              << "; column " << existing.getColumn()
              << "; blinding timestamp " << existing.getBlindingTimestamp().getTime();
          }
        }
      }
      downloads->erase(position);
    } else {
      // Our copy is not pristine: payload will be downloaded
      mDestination->remove(existing);
    }
  }
}

rxcpp::observable<FakeVoid> DownloadProcessor::retrieveFromServer(std::shared_ptr<Progress> progress, std::shared_ptr<Context> ctx, std::shared_ptr<std::unordered_map<RecordDescriptor, EnumerateResult>> downloads) {
  progress->advance("Retrieving from server");
  // Extract download properties into context and local variables
  auto subjects = std::make_shared<std::queue<EnumerateResult>>();
  ctx->descriptors.reserve(downloads->size());
  ctx->sizes.reserve(downloads->size());
  while (!downloads->empty()) {
    auto position = downloads->begin();
    ctx->descriptors.emplace_back(MakeSharedCopy(position->first));
    ctx->sizes.emplace_back(position->second.mFileSize);
    subjects->push(std::move(position->second));

    downloads->erase(position); // Discard source value to reduce memory use
  }
  ctx->streams.resize(subjects->size());

  // Retrieve data for fields that we're updating
  auto retrieveProgress = Progress::Create(subjects->size(), progress->push());
  auto self = SharedFrom(*this);
  return ctx->client->retrieveData2(RxDrain(subjects), ctx->ticket->getTicket(), true)
    .flat_map([self, ctx, retrieveProgress](std::shared_ptr<RetrieveResult> result) {
    return self->processDataChunk(retrieveProgress, ctx, result);
      })
    .op(RxBeforeCompletion([self, ctx, retrieveProgress]() {
    self->processEmptyFiles(retrieveProgress, ctx);
    retrieveProgress->advanceToCompletion();
      }));
}

rxcpp::observable<FakeVoid> DownloadProcessor::processDataChunk(std::shared_ptr<Progress> retrieveProgress, std::shared_ptr<Context> ctx, std::shared_ptr<RetrieveResult> result) {
  auto stream = ctx->streams.at(result->mIndex);
  if (stream == nullptr) {
    // Receiving the first part of the record: open stream now
    auto& descriptor = ctx->descriptors.at(result->mIndex);
    assert(descriptor != nullptr);
    stream = this->openStorageStream(std::move(*descriptor), ctx->sizes.at(result->mIndex), *retrieveProgress);
    descriptor.reset();
    ctx->streams[result->mIndex] = stream;
  } else {
    assert(ctx->descriptors.at(result->mIndex) == nullptr);
  }

  assert(result->mContent);

  auto self = SharedFrom(*this);
  return result->mContent
    ->map([self, stream](const std::string& chunk) {
    if (stream->isCommitted() && chunk.empty()) { //stream->write will throw an error if it has already been commited, but we want to allow an empty chunk
      LOG(LOG_TAG, warning) << "Trying to write empty chunk to record that has already been committed.";
    } else {
      stream->write(chunk, self->mGlobalConfig);
    }
    return FakeVoid();
      });
}

void DownloadProcessor::processEmptyFiles(std::shared_ptr<Progress> retrieveProgress, std::shared_ptr<Context> ctx) {
  for (size_t i = 0U; i < ctx->streams.size(); ++i) {
    if (ctx->streams[i] == nullptr) {
      // We've received no content (chunk) for this download, so it must be an empty file: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2337
      auto& descriptor = ctx->descriptors.at(i);
      assert(descriptor != nullptr);
      ctx->streams[i] = this->openStorageStream(std::move(*descriptor), 0U, *retrieveProgress);
      descriptor.reset();
      ctx->streams[i]->commit(mGlobalConfig);
    } else {
      assert(ctx->descriptors.at(i) == nullptr);
    }
  }
}

std::shared_ptr<DownloadDirectory::RecordStorageStream> DownloadProcessor::openStorageStream(RecordDescriptor descriptor, size_t fileSize, Progress& progress) {
  bool pseudonymisationRequired{ false };
  bool archiveExtractionRequired{ false };
  if (auto columnSpecification = mGlobalConfig->getColumnSpecification(descriptor.getColumn())) {
    if (columnSpecification->getAssociatedShortPseudonymColumn().has_value()) {
      pseudonymisationRequired = true;
    }
    archiveExtractionRequired = columnSpecification->getRequiresDirectory();
  }

  auto result = mDestination->create(std::move(descriptor), pseudonymisationRequired, archiveExtractionRequired, fileSize);
  progress.advance(result->getRelativePath().string());
  return result;
}
