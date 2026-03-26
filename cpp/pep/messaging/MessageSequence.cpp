#include <pep/messaging/MessageSequence.hpp>

#include <pep/async/CreateObservable.hpp>

namespace pep::messaging {

namespace {

void ProvideNextBatch(std::shared_ptr<std::istream> stream, rxcpp::subscriber<MessageSequence> outer) {
  if (!stream->good()) {
    // No more data available: no more batches will be forthcoming
    outer.on_completed();
  } else {
    // Give the outer observable a MessageSequence ("batch") that'll postpone reading the data into memory until someone .subscribe()s to it
    outer.on_next(CreateObservable<std::shared_ptr<std::string>>([stream, outer](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
      auto page = std::make_shared<std::string>(DEFAULT_PAGE_SIZE, '\0');

      // Read data from stream into page
      stream->read(page->data(), static_cast<std::streamsize>(page->size()));
      size_t nRead = static_cast<size_t>(stream->gcount());
      if (nRead != page->size()) {
        // Last data read from stream: ensure we don't pass uninitialized string content to our subscriber
        page->resize(nRead);
      }

      // Provide this page to the (inner) subscriber
      if (nRead > 0) {
        inner.on_next(page);
      }
      inner.on_completed();

      // Now that (the content of) this batch has been processed, provide the next batch to the outer observable
      ProvideNextBatch(stream, outer);
      }));
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
