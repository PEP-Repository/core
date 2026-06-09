#include <pep/weblib/ObservableStream.hpp>

#include <pep/utils/TestError.hpp>
#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <emscripten/em_js.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-map.hpp>

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

EM_JS(EM_VAL /*Promise<string>*/, StreamOutputAsString, (EM_VAL /*ReadableStream*/ streamHandle), { //language=js
  return Emval.toHandle((async () => {
    const stream = Emval.toValue(streamHandle);
    return (await Array.fromAsync(stream)).toString();
  })());
});

EM_JS(EM_VAL /*Promise*/, StreamErrorInConsumer, (EM_VAL /*ReadableStream<number>*/ streamHandle), { //language=js
  return Emval.toHandle((async () => {
    const stream = Emval.toValue(streamHandle);
    for await (const val of stream) {
      if (val === 2) { throw new Error("My consumer error!"); }
    }
  })());
});

// This demonstrates how to loop through a stream, making sure all objects are properly deallocated
//TODO Is there a better way to do this?
EM_JS(EM_VAL /*Promise*/, StreamDeallocErrorInConsumer, (EM_VAL /*ReadableStream<number>*/ streamHandle), { //language=js
  return Emval.toHandle((async () => {
    const stream = Emval.toValue(streamHandle);
    try {
      let i = 0;
      for await (const val of stream.values({preventCancel: true})) {
        try {
          if (i++ === 1) { throw new Error("My consumer error!"); }
        } finally {
          val.delete(); // `using` supported from Node.js 24
        }
      }
    } finally {
      for await (const val of stream) {
        val.delete();
      }
    }
  })());
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

TEST(ObservableStream, basic) {
  auto result = PromiseTest([] {
    val stream = CreateReadableStream(rxcpp::observable<>::range(0, 3).map([](int i) { return val(i); }));
    return val::take_ownership(StreamOutputAsString(stream.as_handle()));
  });
  EXPECT_EQ(result, "0,1,2,3");
}

TEST(ObservableStream, errorInObservable) {
  EXPECT_THROW(PromiseTest([] {
    val stream = CreateReadableStream(rxcpp::observable<>::error<val>(std::make_exception_ptr(TestError())));
    return val::take_ownership(StreamOutputAsString(stream.as_handle()));
  }), TestError) << "Should propagate error from observable to stream reader";
}

TEST(ObservableStream, errorInConsumer) {
  EXPECT_THROW({
    try {
      PromiseTest([] {
        val stream = CreateReadableStream(rxcpp::observable<>::range(0, 3).map([](int i) { return val(i); }));
        return val::take_ownership(StreamErrorInConsumer(stream.as_handle()));
      });
    } catch (const std::runtime_error& e) {
      EXPECT_EQ(e.what(), "Error: My consumer error!"sv);
      throw;
    }
  }, std::runtime_error) << "Should propagate error from consumer and not crash cancelling stream";
}

class TestClass {
  unsigned* numAllocated_;
public:
  explicit TestClass(unsigned* numAllocated) : numAllocated_{numAllocated} { ++*numAllocated_; }
  ~TestClass() { --*numAllocated_; }

  TestClass(const TestClass&) = delete;
  TestClass& operator=(const TestClass&) = delete;
};

EMSCRIPTEN_BINDINGS(TestClass) {
  class_<TestClass>("TestClass");
}

TEST(ObservableStream, dealloc) {
  unsigned numAllocated{};
  EXPECT_THROW(PromiseTest([numAllocated = &numAllocated] {
    val stream = CreateReadableStream(rxcpp::observable<>::range(0, 3)
      .map([numAllocated](int) { return val(new TestClass(numAllocated), allow_raw_pointers{}); }));
    return val::take_ownership(StreamDeallocErrorInConsumer(stream.as_handle()));
  }), std::runtime_error);
  EXPECT_EQ(numAllocated, 0) << "Expected all objects in ReadableStream to be properly deallocated";
}


}
