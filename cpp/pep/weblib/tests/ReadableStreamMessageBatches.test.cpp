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

using namespace emscripten;
using namespace pep::weblib;
using namespace pep::weblib::tests;
using namespace std::literals;
using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

namespace {

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmissing-variable-declarations"
#endif

EM_JS(EM_VAL, PagesAsBytes, (EM_VAL streamHandle), {
  return Emval.toHandle((async () => {
    const pages = await Array.fromAsync(Emval.toValue(streamHandle));
    const joined = new Blob(pages.flatMap((page, i) => i === 0 ? [page] : ['|', page]));
    return new Uint8Array(await joined.arrayBuffer());
  })());
});

EM_JS(EM_VAL, MakeReader, (EM_VAL contentHandle), {
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

EM_JS(EM_VAL, MakeCountingReader, (int size), {
  let remaining = size;
  return Emval.toHandle({
    readCount: 0,
    read() {
      ++this.readCount;
      const length = Math.min(4, remaining);
      remaining -= length;
      return Promise.resolve(length > 0 ? {done: false, value: new Uint8Array(length)} : {done: true, value: undefined});
    },
  });
});

EM_JS(EM_VAL, MakeDeferred, (), {
  let resolve;
  const promise = new Promise(r => resolve = r);
  return Emval.toHandle({promise, resolve});
});

EM_JS(EM_VAL, MakeFailingReader, (), {
  return Emval.toHandle({read: () => Promise.reject(new Error("Stream error!"))});
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

val PagesPromise(val reader, std::size_t pageSize) {
  val stream = CreateReadableByteStream(ReadableStreamToMessageBatches(std::move(reader), pageSize, observe_on_emscripten_main_thread())
                                          .concat()
                                          .map([](const std::shared_ptr<std::string>& page) { return *page; }),
                                        pageSize);
  return val::take_ownership(PagesAsBytes(stream.as_handle()));
}

val MakeStreamReader(const std::string& content) {
  const std::span bytes = pep::ConvertBytes<std::uint8_t>(content);
  val copy = val::global("Uint8Array").new_(typed_memory_view(bytes.size(), bytes.data()));
  return val::take_ownership(MakeReader(copy.as_handle()));
}

std::string ReadStreamPages(std::string content, std::size_t pageSize) {
  return PromiseTest([content = std::move(content), pageSize] {
    return PagesPromise(MakeStreamReader(content), pageSize);
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

TEST(ReadableStreamMessageBatches, binaryContent) {
  const auto content = "a\0b\x01\x7f\xc3\x28"s;
  EXPECT_EQ(ReadStreamPages(content, 16), content);
}

TEST(ReadableStreamMessageBatches, stopsReadingWhenConsumerStops) {
  const std::string readCount = PromiseTest([] {
    const val reader = val::take_ownership(MakeCountingReader(10));
    const val deferred = val::take_ownership(MakeDeferred());
    const rxcpp::composite_subscription outer;
    ReadableStreamToMessageBatches(reader, 4, observe_on_emscripten_main_thread())
        .subscribe(outer, [reader, outer, deferred](const rxcpp::observable<std::shared_ptr<std::string>>& batch) {
          batch.subscribe([](const std::shared_ptr<std::string>&) {}, [reader, outer, deferred] {
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
