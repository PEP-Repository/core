#include <pep/messaging/MessageSequence.hpp>

#include <pep/async/RxSubsequently.hpp>

namespace pep::messaging {

namespace {

/// @brief Produces a MessageSequence ("batch") containing (some) data from an input stream.
/// @param stream The input stream to read from
/// @return A MessageSequence containing at most a single string ("page").
/// @remark Postpones reading data from the input stream until someone .subscribe()s to the MessageSequence
MessageSequence MakeBatch(std::shared_ptr<std::istream> stream) {
  return CreateObservable<std::shared_ptr<std::string>>([stream](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
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

void ProvideBatch(std::shared_ptr<std::istream> stream, rxcpp::subscriber<MessageSequence> outer) {
  if (stream->good()) {
    outer.on_next(MakeBatch(stream) // Read data from the stream into a single MessageSequence ("batch")...
      .op(RxSubsequently([stream, outer]() { ProvideBatch(stream, outer); }))); // ... before continuing with the next batch (if any)
  } else {
    // No more data available: no more batches will be forthcoming
    outer.on_completed();
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
    ProvideBatch(stream, outer);
    });
}

}
