#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxCache.hpp>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

using namespace pep;

namespace {

TEST(RxCache, finishes)
{
  boost::asio::io_context io_context;

  auto finished = std::make_shared<bool>(false);
  CreateRxCache([]() {return rxcpp::observable<>::just(42); })
    ->observe()
    .subscribe(
      [](int i) {ASSERT_TRUE(i = 42) << "Incorrect value emitted"; },
      [](std::exception_ptr ep) {ASSERT_TRUE(false) << "Failed instead of finishing"; },
      [finished]() {*finished = true; }
    );

  io_context.run();

  ASSERT_TRUE(*finished) << "Did not finish";
}

TEST(RxCache, producesSourceValues)
{
  boost::asio::io_context io_context;

  std::vector<int> values { 1, 2, 3 };
  auto emitted = std::make_shared<std::vector<int>>();

  CreateRxCache([values]() {return rxcpp::observable<>::iterate(values); })
    ->observe()
    .subscribe(
      [emitted](int i) {emitted->push_back(i); },
      [](std::exception_ptr ep) {ASSERT_TRUE(false) << "Should not fail"; },
      []() {}
  );

  io_context.run();

  ASSERT_TRUE(values == *emitted) << "Emitted values other than the ones expected";
}

TEST(RxCache, caches)
{
  boost::asio::io_context io_context;

  // Create an observable that raises an error when subscribed multiple times
  auto subscribed = std::make_shared<bool>(false);
  auto singleshot = CreateObservable<int>([subscribed](rxcpp::subscriber<int> subscriber) {
    if (*subscribed) {
      throw std::runtime_error("Subscribed multiple times");
    }
    *subscribed = true;
    subscriber.on_next(42);
    subscriber.on_completed();
  });

  // Cache emissions of the single-shot observable
  auto cache = CreateRxCache([singleshot]() {return singleshot; });

  // Subscribe multiple times to the cache's emissions
  cache->observe().subscribe(
    [](int) {},
    [](std::exception_ptr ep) {ASSERT_TRUE(false) << "Subscription 1 should not fail"; },
    [cache]() {
    cache->observe().subscribe(
      [](int) {},
      [](std::exception_ptr ep) {ASSERT_TRUE(false) << "Subscription 2 should not fail"; },
      [cache]() {
      cache->observe().subscribe(
        [](int) {},
        [](std::exception_ptr ep) {ASSERT_TRUE(false) << "Subscription 3 should not fail"; },
        [cache]() {
      });
    });
  });
  io_context.run();

  // Subscribe once more for good measure
  cache->observe().subscribe(
    [](int) {},
    [](std::exception_ptr ep) {ASSERT_TRUE(false) << "Subscription 4 should not fail"; },
    []() {}
  );
  io_context.run();
}

TEST(RxCache, doesNotCacheErrors)
{
  boost::asio::io_context io_context;

  // Exception class that contains a number that we'll increment for every time it's raised
  struct NumberedError : public std::exception
  {
    const int mNumber;
    explicit NumberedError(int number) : mNumber(number) {}
  };
  auto errnum = std::make_shared<int>(0);

  // Cache the "emissions" of an exception-raising observable
  auto cache = CreateRxCache([errnum]() {
    return CreateObservable<int>([errnum](rxcpp::subscriber<int> subscriber) {
      subscriber.on_error(std::make_exception_ptr(NumberedError(++ * errnum)));
      });
    });

  // Observe cache
  cache->observe().subscribe(
    [](int) {ASSERT_TRUE(false) << "Subscription 1 should not produce a value"; },
    [](std::exception_ptr ep) {
    try {
      std::rethrow_exception(ep);
    }
    catch (const NumberedError& expected) {
      ASSERT_TRUE(expected.mNumber == 1) << "Subscription 1 should produce exception number 1";
    }
    catch (...) { ASSERT_TRUE(false) << "Subscription 1 should have raised exception of type NumberedError"; }
  },
    []() {ASSERT_TRUE(false) << "Subscription 1 should have raised an exception"; }
  );
  io_context.run();

  // Observe cache second time. It should produce a new error; not the one for the first "attempt"
  cache->observe().subscribe(
    [](int) {ASSERT_TRUE(false) << "Subscription 2 should not produce a value"; },
    [](std::exception_ptr ep) {
    try {
      std::rethrow_exception(ep);
    }
    catch (const NumberedError& expected) {
      ASSERT_TRUE(expected.mNumber == 2) << "Subscription 2 should produce exception number 2";
    }
    catch (...) { ASSERT_TRUE(false) << "Subscription 2 should have raised exception of type NumberedError"; }
  },
    []() {ASSERT_TRUE(false) << "Subscription 2 should have raised an exception"; }
  );
  io_context.run();
}

TEST(RxCache, emitsValuesBeforeError)
{
  std::vector<int> values{ 1, 2, 3 };
  auto emitted = std::make_shared<std::vector<int>>();

  // Create an observable that emits values before raising an error.
  auto source = CreateObservable<int>([values](rxcpp::subscriber<int> subscriber) {
    for (auto& i : values) {
      subscriber.on_next(i);
    }
    subscriber.on_error(std::make_exception_ptr(std::runtime_error("Failing after three items")));
    });

  // Ensure that the RxCache also emits the source values before raising the error.
  CreateRxCache([source]() {return source; })
    ->observe()
    .subscribe(
      [emitted](int value) {emitted->push_back(value); },
      [values, emitted](std::exception_ptr ep) {ASSERT_TRUE(values == *emitted) << "Cache should have produced source values"; },
      []() {ASSERT_TRUE(false) << "Cache should have terminated with the source error"; }
    );

  boost::asio::io_context io_context;
  io_context.run();
}

TEST(RxCache, retries)
{
  boost::asio::io_context io_context;

  // Cache an observable that fails for the first subscriber but succeeds for subsequent ones
  auto fail = std::make_shared<bool>(true);
  auto cache = CreateRxCache([fail]() {
    return CreateObservable<int>([fail](rxcpp::subscriber<int> subscriber) {
      if (*fail) {
        *fail = false;
        subscriber.on_error(std::make_exception_ptr(std::runtime_error("Observable's first time failure")));
      }
      else {
        subscriber.on_next(42);
        subscriber.on_completed();
      }
      });
    });

  // Ensure that the cache retries with a new source after encountering an error
  cache->observe().subscribe(
    [](int) {ASSERT_TRUE(false) << "Subscription 1 should not produce a value"; },
    [cache](std::exception_ptr ep) {
    auto emitted = std::make_shared<bool>(false);
    cache->observe().subscribe(
      [emitted](int) {*emitted = true; },
      [](std::exception_ptr ep) { ASSERT_TRUE(false) << "Subscription 2 should not fail"; },
      [emitted]() {ASSERT_TRUE(*emitted) << "Subscription 2 should have emitted a value"; }
    );
  },
    []() {ASSERT_TRUE(false) << "Subscription 1 should not complete"; }
  );

  io_context.run();

  // Try once more for good measure.
  auto emitted = std::make_shared<bool>(false);
  cache->observe().subscribe(
    [emitted](int) {*emitted = true; },
    [](std::exception_ptr ep) { ASSERT_TRUE(false) << "Subscription 3 should not fail"; },
    [emitted]() {ASSERT_TRUE(*emitted) << "Subscription 3 should have emitted a value"; }
  );

  io_context.run();
}

}
