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

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace emscripten;
using namespace pep::weblib;
using namespace pep::weblib::tests;
using namespace std::literals;

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

TEST(BlobMessageBatches, errorFromBlob) {
  EXPECT_THROW({
    try {
      PromiseTest([] { return PagesPromise(val::take_ownership(MakeFailingBlob()), 4); });
    } catch (const std::runtime_error& e) {
      EXPECT_NE(std::string_view(e.what()).find("Blob error!"), std::string_view::npos)
          << "Should include the underlying JS error, got: " << e.what();
      throw;
    }
  }, std::runtime_error) << "Should propagate a failing Blob read";
}

TEST(BlobMessageBatches, zeroPageSize) {
  EXPECT_THROW(PromiseTest([] { return PagesPromise(MakeBlob("abc"), 0); }), std::invalid_argument);
}

}
