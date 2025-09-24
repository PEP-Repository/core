#include <pep/messaging/MessageSequence.hpp>

#include <pep/async/CreateObservable.hpp>

namespace {
#if BUILD_HAS_DEBUG_FLAVOR()
constexpr uint64_t DEFAULT_PAGE_SIZE = 1024 * 1024 / 2; //To make sure it will fit within the reduced MAX_SIZE_OF_MESSAGE for debug builds
#else
constexpr uint64_t DEFAULT_PAGE_SIZE = 1024 * 1024;
#endif
}

namespace pep::messaging {
MessageBatches IStreamToMessageBatches(std::shared_ptr<std::istream> stream) {
  return pep::CreateObservable<MessageSequence>([stream, first = std::make_shared<bool>(true)](rxcpp::subscriber<MessageSequence> subscriber) {
    // Rewind stream to beginning
    if (!*first) {
      stream->seekg(0);
    }
    *first = false;

    // Try to iteratively emit data in page-sized chunks
    // However: there is no guarantee that read() will do this
    while (stream->good()) {
      // Create instance to store next page
      auto page = std::make_shared<std::string>(DEFAULT_PAGE_SIZE, '\0');

      // Read data from stream into page
      stream->read(page->data(), static_cast<std::streamsize>(page->size()));
      size_t nRead = static_cast<size_t>(stream->gcount());
      if (nRead != page->size()) {
        // Last data read from stream: ensure we don't pass uninitialized string content to our subscriber
        page->resize(nRead);
      }

      // Provide this page to the subscriber
      if (nRead > 0) {
        subscriber.on_next(rxcpp::observable<>::just(page));
      }
    }

    subscriber.on_completed();
  });
}
}
