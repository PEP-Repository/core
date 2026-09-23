#include <pep/utils/Defer.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/ScopedResource.hpp>
#include <boost/noncopyable.hpp>
#include <gtest/gtest.h>

namespace {

template <typename TResource>
class InalizationCounter;

template <>
class InalizationCounter<void> : boost::noncopyable {
public:
  using Handler = std::function<void()>;
  using Initializer = Handler;
  using Finalizer = Handler;

private:
  Initializer initialize_;
  Finalizer finalize_;
  int initializationAttempts_ = 0;
  int initializations_ = 0;
  int finalizationAttempts_ = 0;
  int finalizations_ = 0;

public:
  explicit InalizationCounter(Initializer initialize, Finalizer finalize) : initialize_(std::move(initialize)), finalize_(std::move(finalize)) {}

  void initialize() { ++initializationAttempts_; initialize_(); ++initializations_; }
  void finalize() { ++finalizationAttempts_; finalize_(); ++finalizations_; }

  int initializationAttempts() const noexcept { return initializationAttempts_; }
  int initializations() const noexcept { return initializations_; }
  int finalizationAttempts() const noexcept { return finalizationAttempts_; }
  int finalizations() const noexcept { return finalizations_; }
};

template <typename TResource>
class InalizationCounter {
public:
  using Initializer = std::function<TResource()>;
  using Finalizer = std::function<void(const TResource&)>;

private:
  Initializer initialize_;
  Finalizer finalize_;
  int initializationAttempts_ = 0;
  int initializations_ = 0;
  int finalizationAttempts_ = 0;
  int finalizations_ = 0;

public:
  explicit InalizationCounter(Initializer initialize, Finalizer finalize) : initialize_(std::move(initialize)), finalize_(std::move(finalize)) {}

  TResource initialize() {
    ++initializationAttempts_;
    bool successful = true;
    PEP_DEFER(if (successful) ++initializations_);

    try {
      return initialize_();
    }
    catch (...) {
      successful = false;
      throw;
    }
  }

  void finalize(const TResource& resource) { ++finalizationAttempts_; finalize_(resource); ++finalizations_; }

  int initializationAttempts() const noexcept { return initializationAttempts_; }
  int initializations() const noexcept { return initializations_; }
  int finalizationAttempts() const noexcept { return finalizationAttempts_; }
  int finalizations() const noexcept { return finalizations_; }
};


template <pep::AnyResourceSetupFunction TSetup, pep::ResourceTeardownFunctionFor<TSetup> TTeardown>
void TestScopedResource(TSetup&& setup, TTeardown&& teardown) {
  using Resource = pep::ResourceProducedBy<TSetup>;
  using Counter = InalizationCounter<Resource>;

  // Make a counting frontend for the raw setup and teardown functions
  Counter counter(std::forward<TSetup>(setup), std::forward<TTeardown>(teardown));

  try {
    // Make a scoped resource that initializes and finalizes through our Counter frontend methods
    auto scoped = pep::MakeScopedResource(std::bind(&Counter::initialize, &counter), std::bind_front(&Counter::finalize, &counter));

    EXPECT_EQ(0, counter.finalizations()) << "Shouldn't finalize until end of scope";
    EXPECT_EQ(counter.initializationAttempts(), counter.initializations()) << "Shouldn't eat initialization exceptions";
    EXPECT_EQ(1, counter.initializations()) << "Should have initialized once";
  }
  catch (...) {
    ASSERT_NE(counter.initializationAttempts(), counter.initializations()) << "Faulty InalizationCounter test helper";
  }

  EXPECT_EQ(1, counter.initializationAttempts()) << "Should have attempted initialization exactly once";
  EXPECT_EQ(counter.initializations(), counter.finalizations()) << "Should finalize all (completed) initializations";
}

}

TEST(ScopedResource, works) {
  EXPECT_NO_FATAL_FAILURE(TestScopedResource(
    []() {},
    []() {}
  )) << "Failed no-op management";

  EXPECT_NO_FATAL_FAILURE(TestScopedResource(
    []() { throw std::runtime_error("Exception during initialization"); },
    []() {}
  )) << "Failed throwing initialization for void";

  auto value = pep::RandomInteger<int>();
  EXPECT_NO_FATAL_FAILURE(TestScopedResource(
    [value]() { return value; },
    [value](const int& resource) { EXPECT_EQ(resource, value) << "Resource corrupted"; }
  )) << "Failed (random) int management";

  EXPECT_NO_FATAL_FAILURE(TestScopedResource(
    []() -> int { throw std::runtime_error("Exception during initialization"); },
    [](const int&) {}
  )) << "Failed throwing initialization for int";
}
