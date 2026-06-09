#include <pep/weblib/ObservableByteStream.hpp>

#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <emscripten/em_js.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

using namespace emscripten;
using namespace pep;
using namespace pep::weblib;
using namespace pep::weblib::tests;
using namespace std::literals;

namespace {

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

void TestObservableByteStream(size_t chunkSize) {
  auto result = PromiseTest([chunkSize] {
    val stream = CreateReadableByteStream(Words, chunkSize);
    return val::take_ownership(ByteStreamOutputAsString(stream.as_handle()));
  });
  EXPECT_EQ(result, "abcdefghi") << "with chunk size" << chunkSize;
}

TEST(ObservableByteStream, SupportsMultipleBufferSizes) {
  TestObservableByteStream(3U); // basic
  TestObservableByteStream(1U); // autoSmallBuffer
  TestObservableByteStream(4U); // autoLargeBuffer
}

}
