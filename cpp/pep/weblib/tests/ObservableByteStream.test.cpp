#include <pep/weblib/ObservableByteStream.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/weblib/OnEmscriptenThread.hpp>
#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <emscripten/em_js.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-subscribe_on.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

using namespace emscripten;
using namespace pep;
using namespace pep::weblib;
using namespace pep::weblib::tests;
using namespace std::literals;

namespace {

const std::string LOG_TAG = "ObservableByteStream.test";

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmissing-variable-declarations" // For EM_JS parameters
#endif

EM_JS(EM_VAL /*Promise<string>*/, ByteStreamOutputAsString, (EM_VAL /*ReadableStream<Uint8Array<ArrayBuffer>>*/ streamHandle), { //language=js
  return Emval.toHandle((async () => {
    const stream = Emval.toValue(streamHandle);
    let content = "";
    for await (const chunk of stream.pipeThrough(new TextDecoderStream())) {
      content += chunk;
    }
    return content;
  })());
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

// Note: Node.js doesn't seem to allocate a buffer

const auto Words = rxcpp::observable<>::iterate(std::vector<std::string>{"abc", "def", "ghi"});

TEST(ObservableByteStream, basic) {
  auto result = PromiseTest([] {
    val stream = CreateReadableByteStream(Words, 3);
    return val::take_ownership(ByteStreamOutputAsString(stream.as_handle()));
  });
  EXPECT_EQ(result, "abcdefghi");
}

TEST(ObservableByteStream, autoSmallBuffer) {
  auto result = PromiseTest([] {
    val stream = CreateReadableByteStream(Words, 1);
    return val::take_ownership(ByteStreamOutputAsString(stream.as_handle()));
  });
  EXPECT_EQ(result, "abcdefghi");
}

TEST(ObservableByteStream, autoLargeBuffer) {
  auto result = PromiseTest([] {
    val stream = CreateReadableByteStream(Words, 4);
    return val::take_ownership(ByteStreamOutputAsString(stream.as_handle()));
  });
  EXPECT_EQ(result, "abcdefghi");
}

}
