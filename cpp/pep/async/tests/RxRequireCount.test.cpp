#include <pep/async/RxRequireCount.hpp>
#include <pep/async/tests/RxTestUtils.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

TEST(RxRequireCount, Works) {
  using namespace pep;

  boost::asio::io_context io_context;

  EXPECT_NO_THROW(testutils::exhaust(io_context, rxcpp::observable<>::range(1, 5).op(RxRequireCount(3U, 6U))));
  EXPECT_ANY_THROW(testutils::exhaust(io_context, rxcpp::observable<>::range(1, 5).op(RxRequireCount(3U, 4U))));
  EXPECT_ANY_THROW(testutils::exhaust(io_context, rxcpp::observable<>::range(1, 5).op(RxRequireCount(6U, 8U))));

  EXPECT_NO_THROW(testutils::exhaust(io_context, rxcpp::observable<>::range(1, 5).op(RxRequireCount(5U))));
  EXPECT_ANY_THROW(testutils::exhaust(io_context, rxcpp::observable<>::range(1, 5).op(RxRequireCount(4U))));
  EXPECT_ANY_THROW(testutils::exhaust(io_context, rxcpp::observable<>::empty<int>().op(RxRequireCount(4U))));

  EXPECT_NO_THROW(testutils::exhaust(io_context, rxcpp::observable<>::just(1).op(RxRequireNonEmpty())));
  EXPECT_ANY_THROW(testutils::exhaust(io_context, rxcpp::observable<>::empty<int>().op(RxRequireNonEmpty())));

  constexpr auto itemName = "floober in the goober";
  EXPECT_THAT([&] { testutils::exhaust(io_context, rxcpp::observable<>::empty<int>().op(RxGetOne(itemName))); },
    ::testing::ThrowsMessage<std::exception>(::testing::HasSubstr(itemName)));
}
