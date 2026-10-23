#include <pep/weblib/ReadableStreamMessageBatches.hpp>

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
#include <vector>

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

// Creates a reader for a ReadableStream that produces \p content as a single chunk (no chunk if it is empty).
EM_JS(EM_VAL /*ReadableStreamDefaultReader*/, MakeReader, (EM_VAL /*Uint8Array*/ contentHandle), { //language=js
  const content = Emval.toValue(contentHandle);
  return Emval.toHandle(new ReadableStream({
    start(controller) {
      if (content.length > 0) {
        controller.enqueue(content);
      }
      controller.close();
    },
  }).getReader());
});

// Creates a reader for a ReadableStream that produces chunks of the given sizes, with the bytes 'a', 'b', ... 'z', 'a', ...
// running on across chunks, so that a test can tell from the pages which bytes ended up where.
EM_JS(EM_VAL /*ReadableStreamDefaultReader*/, MakeChunksReader, (EM_VAL /*number[]*/ chunkSizesHandle), { //language=js
  const chunkSizes = Emval.toValue(chunkSizesHandle);
  let next = 0;
  return Emval.toHandle(new ReadableStream({
    start(controller) {
      for (const size of chunkSizes) {
        controller.enqueue(Uint8Array.from({length: size}, () => 'a'.charCodeAt(0) + next++ % 26));
      }
      controller.close();
    },
  }).getReader());
});

// Creates a reader-like object that produces \p size bytes in chunks of \p chunkSize and counts its read() calls,
// so a test can tell how much was read.
EM_JS(EM_VAL /*reader-like*/, MakeCountingReader, (int size, int chunkSize), { //language=js
  let remaining = size;
  return Emval.toHandle({
    readCount: 0,
    read() {
      ++this.readCount;
      const length = Math.min(chunkSize, remaining);
      remaining -= length;
      return Promise.resolve(length > 0 ? {done: false, value: new Uint8Array(length)} : {done: true, value: undefined});
    },
  });
});

// Creates a Promise plus the resolve function for it, so C++ can settle it when it is ready.
EM_JS(EM_VAL /*{promise, resolve}*/, MakeDeferred, (), { //language=js
  let resolve;
  const promise = new Promise(r => resolve = r);
  return Emval.toHandle({promise, resolve});
});

// Creates a reader-like object whose read() always rejects with an error.
EM_JS(EM_VAL /*reader-like*/, MakeFailingReader, (), { //language=js
  return Emval.toHandle({read: () => Promise.reject(new Error("Stream error!"))});
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

// Converts the reader into a page stream and passes it to PagesAsBytes for collection.
val PagesPromise(val reader, std::size_t pageSize) {
  val stream = CreateReadableByteStream(ReadableStreamToMessageBatches(std::move(reader), pageSize, observe_on_emscripten_main_thread())
                                          .concat() // Exhaust each batch before the next one is produced
                                          .map([](const std::shared_ptr<std::string>& page) { return *page; }),
                                        pageSize);
  return val::take_ownership(PagesAsBytes(stream.as_handle()));
}

/// Creates a reader for a ReadableStream producing exactly the bytes of \p content, as File.stream() would.
/// (Passing a std::string as a val would instead produce a JS string, decoding the bytes as UTF-8.)
val MakeStreamReader(const std::string& content) {
  const std::span bytes = pep::ConvertBytes<std::uint8_t>(content);
  // Copies out of wasm memory, which the view merely aliases
  val copy = val::global("Uint8Array").new_(typed_memory_view(bytes.size(), bytes.data()));
  return val::take_ownership(MakeReader(copy.as_handle()));
}

/// Creates a stream from \p content, reads it in pages of \p pageSize, and returns the pages joined by '|'.
std::string ReadStreamPages(std::string content, std::size_t pageSize) {
  return PromiseTest([content = std::move(content), pageSize] {
    return PagesPromise(MakeStreamReader(content), pageSize);
  });
}

/// Reads a stream with chunks of \p chunkSizes in pages of \p pageSize, and returns the pages joined by '|'.
std::string ReadChunks(const std::vector<int>& chunkSizes, std::size_t pageSize) {
  return PromiseTest([chunkSizes, pageSize] {
    val sizes = val::array(chunkSizes);
    return PagesPromise(val::take_ownership(MakeChunksReader(sizes.as_handle())), pageSize);
  });
}

TEST(ReadableStreamMessageBatches, singlePage) {
  EXPECT_EQ(ReadStreamPages("hello", 16), "hello") << "A stream smaller than a page should produce a single page";
}

TEST(ReadableStreamMessageBatches, pageBoundaries) {
  EXPECT_EQ(ReadStreamPages("abcdefg", 3), "abc|def|g") << "Should read the stream in pages of at most pageSize";
}

TEST(ReadableStreamMessageBatches, exactlyFullPages) {
  EXPECT_EQ(ReadStreamPages("abcdef", 3), "abc|def") << "Should not produce a trailing empty page";
}

TEST(ReadableStreamMessageBatches, singleBytePages) {
  EXPECT_EQ(ReadStreamPages("abc", 1), "a|b|c");
}

TEST(ReadableStreamMessageBatches, emptyStream) {
  EXPECT_EQ(ReadStreamPages("", 8), "") << "An empty stream should produce no data";
}

TEST(ReadableStreamMessageBatches, manyPages) {
  std::string content;
  for (std::size_t i = 0; i < 1000; ++i) {
    content += static_cast<char>('a' + i % 26);
  }
  const std::string pages = ReadStreamPages(content, 7);

  std::string joined = pages;
  std::erase(joined, '|');
  EXPECT_EQ(joined, content) << "Should read the whole stream, in order";
  EXPECT_EQ(std::ranges::count(pages, '|'), static_cast<std::ptrdiff_t>((content.size() + 6) / 7) - 1)
      << "Should split the stream into pageSize-sized pages";
}

TEST(ReadableStreamMessageBatches, combineSmallChunks) {
  EXPECT_EQ(ReadChunks({1, 1, 1, 1, 1, 1}, 4), "abcd|ef") << "Should combine chunks smaller than a page";
}

TEST(ReadableStreamMessageBatches, chunksAcrossPages) {
  EXPECT_EQ(ReadChunks({3, 3, 9, 1}, 4), "abcd|efgh|ijkl|mnop") << "A chunk may end in, and span, several pages";
}

TEST(ReadableStreamMessageBatches, emptyChunks) {
  EXPECT_EQ(ReadChunks({0, 2, 0, 0, 2, 0}, 3), "abc|d") << "Should skip empty chunks";
}

TEST(ReadableStreamMessageBatches, binaryContent) {
  // Embedded NUL and high bytes must survive the Uint8Array -> std::string conversion.
  // \xc3\x28 is invalid UTF-8, so it would be mangled if anything went through a JS string.
  const auto content = "a\0b\x01\x7f\xc3\x28"s;
  EXPECT_EQ(ReadStreamPages(content, 16), content);
}

TEST(ReadableStreamMessageBatches, stopsReadingWhenConsumerStops) {
  // A consumer that exhausts one batch and then unsubscribes should leave the remaining pages unread,
  // rather than the next page being fetched in anticipation.
  // The reader is created (and read) on the main thread, so the count is reported back as the
  // promise's value rather than by inspecting the val from the test's own thread.
  constexpr int ChunkSize = 4;
  constexpr std::size_t PageSize = ChunkSize; // So that each read() fills exactly one page
  const std::string readCount = PromiseTest([] {
    const val reader = val::take_ownership(MakeCountingReader(10, ChunkSize)); // 3 pages
    const val deferred = val::take_ownership(MakeDeferred());
    const rxcpp::composite_subscription outer;
    ReadableStreamToMessageBatches(reader, PageSize, observe_on_emscripten_main_thread())
        .subscribe(outer, [reader, outer, deferred](const rxcpp::observable<std::shared_ptr<std::string>>& batch) {
          batch.subscribe([](const std::shared_ptr<std::string>&) {}, [reader, outer, deferred] {
            // The first batch is exhausted: take no more, and report how much was read
            outer.unsubscribe();
            deferred["resolve"](val(std::to_string(reader["readCount"].as<int>())));
          });
        });
    return deferred["promise"];
  });
  EXPECT_EQ(readCount, "1") << "Exhausting one batch should not read any further page";
}

TEST(ReadableStreamMessageBatches, errorFromStream) {
  EXPECT_THAT([] { PromiseTest([] { return PagesPromise(val::take_ownership(MakeFailingReader()), 4); }); },
              ThrowsMessage<std::runtime_error>(HasSubstr("Stream error!")))
      << "Should propagate a failing stream read, including the underlying JS error";
}

TEST(ReadableStreamMessageBatches, zeroPageSize) {
  EXPECT_THROW(PromiseTest([] { return PagesPromise(MakeStreamReader("abc"), 0); }), std::invalid_argument);
}

}
