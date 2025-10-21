#include <pep/async/RxRequireCount.hpp>
#include <pep/async/tests/RxTestUtils.hpp>

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

  constexpr auto item_name = "floober in the goober";
  try {
    testutils::exhaust(io_context, rxcpp::observable<>::empty<int>().op(RxGetOne(item_name)));
  }
  catch (const std::exception& exception) {
    std::string what = exception.what();
    EXPECT_NE(what.find(item_name), std::string::npos);
  }
}
