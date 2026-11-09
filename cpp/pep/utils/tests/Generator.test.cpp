#include <pep/utils/Generator.hpp>

#include <pep/utils/TestError.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

using namespace pep;

namespace {

static_assert(std::ranges::input_range<Generator<int>>);
static_assert(std::ranges::view<Generator<int>>);
static_assert(std::same_as<std::ranges::range_reference_t<Generator<int>>, int&&>);
static_assert(std::same_as<std::ranges::range_value_t<Generator<int>>, int>);

Generator<int> CountTo(int n) {
  for (int i = 1; i <= n; ++i) {
    co_yield i;
  }
}

Generator<int> Nothing() {
  co_return;
}

struct CopyCounter {
  size_t* copies;

  explicit CopyCounter(size_t& copies) : copies(&copies) {}
  CopyCounter(const CopyCounter& other) : copies(other.copies) { ++*copies; }
  CopyCounter(CopyCounter&&) noexcept = default;
  CopyCounter& operator=(const CopyCounter&) = delete;
  CopyCounter& operator=(CopyCounter&&) = delete;
  ~CopyCounter() = default;
};

TEST(Generator, ProducesYieldedValues) {
  EXPECT_EQ(CountTo(5) | std::ranges::to<std::vector>(), (std::vector{ 1, 2, 3, 4, 5 }));
}

TEST(Generator, ProducesNothingForEmptyBody) {
  EXPECT_TRUE((Nothing() | std::ranges::to<std::vector>()).empty());
  EXPECT_TRUE((CountTo(0) | std::ranges::to<std::vector>()).empty());
}

TEST(Generator, IsLazy) {
  std::vector<std::string> log;
  //NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines) `log` stays in scope
  auto generate = [&log] -> Generator<int> {
    log.emplace_back("start");
    co_yield 1;
    log.emplace_back("after 1");
    co_yield 2;
    log.emplace_back("end");
  };

  auto generator = generate();
  EXPECT_TRUE(log.empty()) << "Body should not run before iteration starts";

  auto it = generator.begin();
  EXPECT_EQ(log, (std::vector<std::string>{ "start" }));
  ASSERT_NE(it, generator.end());
  EXPECT_EQ(*it, 1);

  ++it;
  EXPECT_EQ(log, (std::vector<std::string>{ "start", "after 1" }));
  ASSERT_NE(it, generator.end());
  EXPECT_EQ(*it, 2);

  ++it;
  EXPECT_EQ(log, (std::vector<std::string>{ "start", "after 1", "end" }));
  EXPECT_EQ(it, generator.end());
}

TEST(Generator, CopiesLvaluesButNotRvalues) {
  size_t copies = 0;
  //NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines) `copies` stays in scope
  auto generate = [&copies] -> Generator<CopyCounter> {
    co_yield CopyCounter(copies); // rvalue: not copied
    CopyCounter lvalue(copies);
    co_yield lvalue; // lvalue: copied
  };

  auto generator = generate();
  auto it = generator.begin();
  EXPECT_EQ(copies, 0U);
  ++it;
  EXPECT_EQ(copies, 1U);
  ++it;
  EXPECT_EQ(it, generator.end());
}

TEST(Generator, AllowsMovingValuesOut) {
  auto generate = [] -> Generator<std::unique_ptr<int>> {
    co_yield std::make_unique<int>(1);
    auto two = std::make_unique<int>(2);
    co_yield std::move(two);
  };

  std::vector<int> values;
  for (auto&& ptr : generate()) {
    std::unique_ptr<int> moved = std::move(ptr);
    ASSERT_NE(moved, nullptr);
    values.push_back(*moved);
  }
  EXPECT_EQ(values, (std::vector{ 1, 2 }));
}

TEST(Generator, ConvertsYieldedValues) {
  auto generate = [] -> Generator<std::string> {
    co_yield "literal";
    const char* pointer = "pointer";
    co_yield pointer;
  };
  EXPECT_EQ(generate() | std::ranges::to<std::vector>(), (std::vector<std::string>{ "literal", "pointer" }));
}

TEST(Generator, PropagatesExceptions) {
  auto throwImmediately = [] -> Generator<int> {
    throw TestError{};
    co_return;
  };
  auto generator = throwImmediately();
  EXPECT_THROW(generator.begin(), TestError);

  auto throwLater = [] -> Generator<int> {
    co_yield 1;
    throw TestError{};
  };
  auto later = throwLater();
  auto it = later.begin();
  ASSERT_NE(it, later.end());
  EXPECT_EQ(*it, 1);
  EXPECT_THROW(++it, TestError);
  EXPECT_EQ(it, later.end()) << "Generator should be finished after an exception";
}

TEST(Generator, DestroysFrameWhenAbandoned) {
  auto resource = std::make_shared<int>(0);
  std::weak_ptr<int> weak = resource;
  {
    auto generator = [](std::shared_ptr<int> held) -> Generator<int> {
      for (;;) {
        co_yield ++*held;
      }
    }(std::move(resource));
    auto it = generator.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    EXPECT_FALSE(weak.expired()) << "Coroutine frame should keep its argument alive";
  }
  EXPECT_TRUE(weak.expired()) << "Destroying the generator should destroy its coroutine frame";
}

TEST(Generator, SupportsNestedGenerators) {
  auto outer = [] -> Generator<int> {
    co_yield 0;
    for (auto i : CountTo(3)) {
      co_yield i;
    }
    co_yield 4;
  };
  EXPECT_EQ(outer() | std::ranges::to<std::vector>(), (std::vector{ 0, 1, 2, 3, 4 }));
}

TEST(Generator, ComposesWithViews) {
  auto doubled = CountTo(4)
    | std::views::filter([](int i) { return i % 2 == 0; })
    | std::views::transform([](int i) { return i * 10; })
    | std::ranges::to<std::vector>();
  EXPECT_EQ(doubled, (std::vector{ 20, 40 }));
}

TEST(Generator, CanBeMoved) {
  auto source = CountTo(2);
  auto destination = std::move(source);
  EXPECT_EQ(destination | std::ranges::to<std::vector>(), (std::vector{ 1, 2 }));

  auto assigned = CountTo(7);
  assigned = CountTo(1);
  EXPECT_EQ(assigned | std::ranges::to<std::vector>(), (std::vector{ 1 }));
}

TEST(Generator, CanOnlyBeIteratedOnce) {
  auto generator = CountTo(2);
  [[maybe_unused]] auto it = generator.begin();
  EXPECT_THROW(generator.begin(), std::logic_error);
}

}
