#include <pep/async/RxBeforeTermination.hpp>
#include <pep/async/tests/RxTestUtils.hpp>

#include <stdexcept>

namespace {

class HandlerError : public std::runtime_error {
public:
  HandlerError() : std::runtime_error("handler error") {}
};

class SourceError : public std::runtime_error {
public:
  SourceError() : std::runtime_error("source error") {}
};

/// The terminal notification that a subscriber received, and the exception (if any) that escaped
/// from the subscription instead of being reported to it.
struct Termination {
  bool completed = false;
  std::exception_ptr reportedError;
  std::exception_ptr escapedException;
};

/// Subscribes to an observable that terminates synchronously, and reports how it terminated.
Termination Terminate(rxcpp::observable<int> items) {
  Termination result;

  try {
    items.subscribe(
      [](int) {/*ignore*/},
      [&result](std::exception_ptr error) { result.reportedError = error; },
      [&result]() { result.completed = true; });
  }
  catch (...) {
    result.escapedException = std::current_exception();
  }

  return result;
}

}

TEST(RxBeforeTermination, InvokesHandlerBeforeTerminalNotification) {
  using namespace pep;

  boost::asio::io_context io_context;

  std::optional<std::optional<std::exception_ptr>> termination;
  auto items = testutils::exhaust(io_context, rxcpp::observable<>::range(1, 3)
    .op(RxBeforeTermination([&termination](std::optional<std::exception_ptr> error) { termination = error; })));

  EXPECT_EQ(*items, (std::vector<int>{1, 2, 3}));
  ASSERT_TRUE(termination.has_value());
  EXPECT_FALSE(termination->has_value()); // Terminated without an error

  termination.reset();
  EXPECT_THROW(testutils::exhaust(io_context, rxcpp::observable<>::error<int>(SourceError())
    .op(RxBeforeTermination([&termination](std::optional<std::exception_ptr> error) { termination = error; }))),
    SourceError);
  ASSERT_TRUE(termination.has_value());
  EXPECT_TRUE(termination->has_value());
}

/// A handler that throws while the observable completes must be reported to the subscriber as an error,
/// and must not escape into whatever invoked the on_completed: see the remark on
/// RxBeforeTerminationSubscriberFactory. Note that rxcpp's tap<> lets it escape.
TEST(RxBeforeTermination, ReportsThrowingCompletionHandlerToSubscriber) {
  using namespace pep;

  auto termination = Terminate(rxcpp::observable<>::range(1, 3)
    .op(RxBeforeTermination([](std::optional<std::exception_ptr>) { throw HandlerError(); })));

  EXPECT_FALSE(termination.completed);
  EXPECT_FALSE(termination.escapedException) << "Exception escaped instead of being reported to the subscriber";
  ASSERT_TRUE(termination.reportedError);
  EXPECT_THROW(std::rethrow_exception(termination.reportedError), HandlerError);
}

/// When the observable is already terminating with an error, that error is the one the subscriber
/// needs: a handler that throws on top of it must not replace or suppress it.
TEST(RxBeforeTermination, KeepsSourceErrorWhenHandlerThrows) {
  using namespace pep;

  auto termination = Terminate(rxcpp::observable<>::error<int>(SourceError())
    .op(RxBeforeTermination([](std::optional<std::exception_ptr>) { throw HandlerError(); })));

  EXPECT_FALSE(termination.completed);
  EXPECT_FALSE(termination.escapedException) << "Exception escaped instead of being reported to the subscriber";
  ASSERT_TRUE(termination.reportedError);
  EXPECT_THROW(std::rethrow_exception(termination.reportedError), SourceError);
}
