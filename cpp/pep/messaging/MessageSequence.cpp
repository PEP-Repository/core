#include <pep/messaging/MessageSequence.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Defer.hpp>

#include <rxcpp/operators/rx-concat.hpp>

namespace pep::messaging {

namespace {

MessageSequence MakeBatch(std::shared_ptr<std::istream> stream) {
  return CreateObservable<std::shared_ptr<std::string>>([stream](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {

    /* If caller schedules the next batch e.g. using a .tap operator (on our return value), rxcpp runs the scheduling code
     * while this lambda is still running. Such recursive (for every batch) calls  would (presumably) cause trouble such as
     * stack overflows for large istreams.
     * We prevent this by asserting that the caller employs a non-recursive technique to schedule followup batches.
     */
#if BUILD_HAS_DEBUG_FLAVOR()
    thread_local size_t recursions = 0U;
    [[maybe_unused]] auto current = recursions++;
    PEP_DEFER(--recursions);
    assert(current == 0U && "Eliminate recursion");
#endif

    // Read data from stream into page
    auto page = std::make_shared<std::string>(DEFAULT_PAGE_SIZE, '\0');
    stream->read(page->data(), static_cast<std::streamsize>(page->size()));
    size_t nRead = static_cast<size_t>(stream->gcount());

    if (nRead > 0) { // Don't pass empty data to our subscriber
      page->resize(nRead); // Only pass filled substring to our subscriber
      inner.on_next(page);
    }
    inner.on_completed(); // Each batch consists of (a maximum of) a single page
    });
}

void ProvideNextBatch(std::shared_ptr<std::istream> stream, rxcpp::subscriber<MessageSequence> subscriber);

MessageSequence TriggerNextBatch(std::shared_ptr<std::istream> stream, rxcpp::subscriber<MessageSequence> outer) {
  return CreateObservable<std::shared_ptr<std::string>>([stream, outer](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
    ProvideNextBatch(stream, outer);
    inner.on_completed();
    });
}

void ProvideNextBatch(std::shared_ptr<std::istream> stream, rxcpp::subscriber<MessageSequence> subscriber) {
  if (!stream->good()) {
    // No more data available: no more batches will be forthcoming
    subscriber.on_completed();
  } else {
    // (Ab)use the "concat" operator to ensure that...
    subscriber.on_next(MakeBatch(stream) // ... the MessageSequence ("batch") is fully exhausted before...
      .concat(TriggerNextBatch(stream, subscriber)) // ...we (non-recursively) trigger production of the next batch
    );
  }
}

}

#if BUILD_HAS_DEBUG_FLAVOR()
extern const uint64_t DEFAULT_PAGE_SIZE = 1024 * 1024 / 2; //To make sure it will fit within the reduced MAX_SIZE_OF_MESSAGE for debug builds
#else
extern const uint64_t DEFAULT_PAGE_SIZE = 1024 * 1024;
#endif

MessageBatches IStreamToMessageBatches(std::shared_ptr<std::istream> stream) {
  return pep::CreateObservable<MessageSequence>([stream](rxcpp::subscriber<MessageSequence> outer) {
    ProvideNextBatch(stream, outer);
    });
}

}
