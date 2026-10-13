#include <pep/weblib/BlobMessageBatches.hpp>

#include <pep/utils/CollectionUtils.hpp>
#include <pep/weblib/ObservableByteStream.hpp>
#include <pep/weblib/OnEmscriptenThread.hpp>
#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <emscripten/em_js.h>
#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-map.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

using namespace emscripten;
using namespace pep::weblib;
using namespace pep::weblib::tests;
using namespace std::literals;
using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

namespace {

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmissing-variable-declarations" // For EM_JS parameters
#endif

/// Collects all pages from a JavaScript ReadableStream and joins them with '|'.
/// Stays in bytes throughout, like production, so data that is not valid UTF-8 survives.
/// Returns the resulting JavaScript Promise as an Emscripten EM_VAL.
EM_JS(EM_VAL /*Promise<Uint8Array>*/, PagesAsBytes, (EM_VAL /*ReadableStream<Uint8Array>*/ streamHandle), { //language=js
  return Emval.toHandle((async () => {
    const pages = await Array.fromAsync(Emval.toValue(streamHandle));
    const joined = new Blob(pages.flatMap((page, i) => i === 0 ? [page] : ['|', page]));
    return new Uint8Array(await joined.arrayBuffer());
  })());
});

// Creates a Blob-like object that counts its slice() calls, so a test can tell how much was read.
EM_JS(EM_VAL /*Blob-like*/, MakeCountingBlob, (double size), { //language=js
  return Emval.toHandle({
    size,
    sliceCount: 0,
    slice(start, end) {
      ++this.sliceCount;
      return {arrayBuffer: () => Promise.resolve(new ArrayBuffer(end - start))};
    },
  });
});

// Creates a Promise plus the resolve function for it, so C++ can settle it when it is ready.
EM_JS(EM_VAL /*{promise, resolve}*/, MakeDeferred, (), { //language=js
  let resolve;
  const promise = new Promise(r => resolve = r);
  return Emval.toHandle({promise, resolve});
});

// Creates a Blob-like object whose arrayBuffer() always rejects with an error.
EM_JS(EM_VAL /*Blob-like*/, MakeFailingBlob, (), { //language=js
  return Emval.toHandle({
    size: 10,
    slice: () => ({arrayBuffer: () => Promise.reject(new Error("Blob error!"))}),
  });
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

// Converts the blob into a page stream and passes it to PagesAsString for collection.
val PagesPromise(val blob, std::size_t pageSize) {
  val stream = CreateReadableByteStream(BlobToMessageBatches(std::move(blob), pageSize, observe_on_emscripten_main_thread())
                                          .concat() // Exhaust each batch before the next one is produced
                                          .map([](const std::shared_ptr<std::string>& page) { return *page; }),
                                        pageSize);
  return val::take_ownership(PagesAsBytes(stream.as_handle()));
}

/// Creates a Blob holding exactly the bytes of \p content, as a File read from disk would.
/// (Passing a std::string as a val would instead produce a JS string, decoding the bytes as UTF-8.)
val MakeBlob(const std::string& content) {
  const std::span bytes = pep::ConvertBytes<std::uint8_t>(content);
  val parts = val::array();
  // Copies out of wasm memory, which the view merely aliases
  parts.call<void>("push", val::global("Uint8Array").new_(typed_memory_view(bytes.size(), bytes.data())));
  return val::global("Blob").new_(parts);
}

/// Creates a Blob from \p content, reads it in pages of \p pageSize, and returns the pages joined by '|'.
std::string ReadBlobPages(std::string content, std::size_t pageSize) {
  return PromiseTest([content = std::move(content), pageSize] {
    return PagesPromise(MakeBlob(content), pageSize);
  });
}

TEST(BlobMessageBatches, singlePage) {
  EXPECT_EQ(ReadBlobPages("hello", 16), "hello") << "A Blob smaller than a page should produce a single page";
}

TEST(BlobMessageBatches, pageBoundaries) {
  EXPECT_EQ(ReadBlobPages("abcdefg", 3), "abc|def|g") << "Should read the Blob in pages of at most pageSize";
}

TEST(BlobMessageBatches, exactlyFullPages) {
  EXPECT_EQ(ReadBlobPages("abcdef", 3), "abc|def") << "Should not produce a trailing empty page";
}

TEST(BlobMessageBatches, singleBytePages) {
  EXPECT_EQ(ReadBlobPages("abc", 1), "a|b|c");
}

TEST(BlobMessageBatches, emptyBlob) {
  EXPECT_EQ(ReadBlobPages("", 8), "") << "An empty Blob should produce no data";
}

TEST(BlobMessageBatches, manyPages) {
  std::string content;
  for (std::size_t i = 0; i < 1000; ++i) {
    content += static_cast<char>('a' + i % 26);
  }
  const std::string pages = ReadBlobPages(content, 7);

  std::string joined = pages;
  std::erase(joined, '|');
  EXPECT_EQ(joined, content) << "Should read the whole Blob, in order";
  EXPECT_EQ(std::ranges::count(pages, '|'), static_cast<std::ptrdiff_t>((content.size() + 6) / 7) - 1)
      << "Should split the Blob into pageSize-sized pages";
}

TEST(BlobMessageBatches, binaryContent) {
  // Embedded NUL and high bytes must survive the ArrayBuffer -> std::string conversion.
  // \xc3\x28 is invalid UTF-8, so it would be mangled if anything went through a JS string.
  const auto content = "a\0b\x01\x7f\xc3\x28"s;
  EXPECT_EQ(ReadBlobPages(content, 16), content);
}

TEST(BlobMessageBatches, stopsReadingWhenConsumerStops) {
  // A consumer that exhausts one batch and then unsubscribes should leave the remaining pages unread,
  // rather than the next page being fetched in anticipation.
  // The Blob is created (and read) on the main thread, so the count is reported back as the
  // promise's value rather than by inspecting the val from the test's own thread.
  const std::string sliceCount = PromiseTest([] {
    const val blob = val::take_ownership(MakeCountingBlob(10)); // 3 pages at pageSize 4
    const val deferred = val::take_ownership(MakeDeferred());
    const rxcpp::composite_subscription outer;
    BlobToMessageBatches(blob, 4, observe_on_emscripten_main_thread())
        .subscribe(outer, [blob, outer, deferred](const rxcpp::observable<std::shared_ptr<std::string>>& batch) {
          batch.subscribe([](const std::shared_ptr<std::string>&) {}, [blob, outer, deferred] {
            // The first batch is exhausted: take no more, and report how much was read
            outer.unsubscribe();
            deferred["resolve"](val(std::to_string(blob["sliceCount"].as<int>())));
          });
        });
    return deferred["promise"];
  });
  EXPECT_EQ(sliceCount, "1") << "Exhausting one batch should not read any further page";
}

TEST(BlobMessageBatches, errorFromBlob) {
  EXPECT_THAT([] { PromiseTest([] { return PagesPromise(val::take_ownership(MakeFailingBlob()), 4); }); },
              ThrowsMessage<std::runtime_error>(HasSubstr("Blob error!")))
      << "Should propagate a failing Blob read, including the underlying JS error";
}

TEST(BlobMessageBatches, zeroPageSize) {
  EXPECT_THROW(PromiseTest([] { return PagesPromise(MakeBlob("abc"), 0); }), std::invalid_argument);
}

}
