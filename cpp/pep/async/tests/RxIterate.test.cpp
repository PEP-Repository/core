#include <gtest/gtest.h>

#include <numeric>

#include <pep/async/RxMoveIterate.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Shared.hpp>

namespace {

class TestContainer {
public:
  using Item = unsigned;
  static constexpr Item ITEM_COUNT{100};

  using Items = std::vector<Item>;
  using value_type = typename Items::value_type;
  using iterator = typename Items::iterator;
  using const_iterator = typename Items::const_iterator;

private:
  size_t& mCounter;
  Items mItems;

public:
  explicit TestContainer(size_t& counter)
    : mCounter(counter), mItems(ITEM_COUNT) {
    ++mCounter;
    std::iota(mItems.begin(), mItems.end(), Item{});
  }

  TestContainer(const TestContainer& other)
    : mCounter(other.mCounter), mItems(other.mItems) {
    ++mCounter;
  }

  TestContainer(TestContainer&& other) noexcept
    : mCounter(other.mCounter), mItems(std::move(other.mItems)) {
    // Don't increase mCounter: we're not copying the items
  }

  inline iterator begin() noexcept { return mItems.begin(); }
  inline const_iterator begin() const noexcept { return mItems.begin(); }
  inline const_iterator cbegin() const noexcept { return mItems.cbegin(); }

  inline iterator end() noexcept { return mItems.end(); }
  inline const_iterator end() const noexcept { return mItems.end(); }
  inline const_iterator cend() const noexcept { return mItems.cend(); }
};

void TestIterationNumberOfCopies(const std::function<rxcpp::observable<TestContainer::Item>(const TestContainer&)>& iterate) {
  size_t number_of_containers = 0U;
  TestContainer original(number_of_containers);
  ASSERT_EQ(number_of_containers, 1U) << "Test class doesn't count instances correctly";

  iterate(original)
    .subscribe(
      [](TestContainer::Item) { /* ignore */ },
      [](std::exception_ptr error) { FAIL() << "Iteration produced an exception: " << pep::GetExceptionMessage(error); },
      []() {}
  );

  // We'll allow for a single copy to be made, e.g. to create a shared<TestContainer> copy of our original
  const size_t MAX_NUMBER_OF_CONTAINERS = 2U;
  ASSERT_LE(number_of_containers, MAX_NUMBER_OF_CONTAINERS) << "Iteration created excessive numbers of copies of the iterable container";
}


TEST(RxIterate, NumberOfCopies)
{
  // Check that RxMoveIterate does not copy (it did in the past)
  TestIterationNumberOfCopies([](const TestContainer& container) {return pep::RxMoveIterate(container); });
}

}
