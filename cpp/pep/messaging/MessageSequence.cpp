#include <pep/messaging/MessageSequence.hpp>

#include <pep/async/RxSubsequently.hpp>

namespace pep::messaging {

namespace {

/// @brief Produces a MessageSequence ("batch") containing (some) data from an input stream.
/// @param stream The input stream to read from
/// @return A MessageSequence containing at most a single string ("page").
/// @remark Postpones reading data from the input stream until someone .subscribe()s to the MessageSequence
MessageSequence MakeBatch(std::shared_ptr<std::istream> stream) {
  assert(stream->good());

  return CreateObservable<std::shared_ptr<std::string>>([stream](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
    // Read data from stream into page
    assert(stream->good());
    auto page = std::make_shared<std::string>(DEFAULT_PAGE_SIZE, '\0');
    stream->read(page->data(), static_cast<std::streamsize>(page->size()));
    size_t nRead = static_cast<size_t>(stream->gcount());

    if (nRead > 0) { // Don't pass empty data to our subscriber
      page->resize(nRead); // Only pass the data to our subscriber that we extracted from the stream
      inner.on_next(page);
    }
    inner.on_completed(); // Each batch consists of (a maximum of) a single page
    });
}

void ProvideBatch(std::shared_ptr<std::istream> stream, rxcpp::subscriber<MessageSequence> outer) {
  // Check order (bad then eof then fail) was cargo culted from https://en.cppreference.com/w/cpp/io/basic_ios/fail.html

  if (stream->bad()) {
    outer.on_error(std::make_exception_ptr(std::runtime_error("Can't read message batches from bad stream")));
  }
  else if (stream->eof()) {
    // Failbit may be set as well "if the end-of-file condition occurs on the input stream before all requested
    // characters could be extracted": see https://en.cppreference.com/w/cpp/io/ios_base/iostate.html
    outer.on_completed();
  }
  else if (stream->fail()) {
    outer.on_error(std::make_exception_ptr(std::runtime_error("Can't read message batches from failed stream")));
  }
  else {
    outer.on_next(MakeBatch(stream) // Provide data from the stream as a single MessageSequence ("batch")...
      .op(RxSubsequently([stream, outer]() { // ...that must be exhausted before...
        ProvideBatch(stream, outer); // ...continuing with the next batch (if any)
        })));
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
