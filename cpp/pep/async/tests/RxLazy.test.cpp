#include <gtest/gtest.h>

#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-switch_if_empty.hpp>

#include <pep/async/RxLazy.hpp>
#include <pep/async/RxTestUtils.hpp>
#include <pep/async/OnAsio.hpp>

namespace pep {

//TEST(RxLazy, onErrorValgrind) 
//{
//  // Valgrind detects a read from uninitialized memory in the "onError"
//  // test below. The following tests whether observable::error is to blame.
//  rxcpp::observable<>::error<int>(std::runtime_error("An error"))
//    .as_blocking().subscribe(
//       [](int i){ ASSERT_TRUE(false); },
//       [](std::exception_ptr ep) { 
//       
//       },
//       [](){ ASSERT_TRUE(false); }
//  );
//}

TEST(RxLazy, onNextComplete)
{
  boost::asio::io_service io_service;

  EXPECT_EQ(std::vector<int>{5},
    *testutils::exhaust<int>(io_service,
      pep::RxLazy<int>([&io_service](){ 
      return rxcpp::observable<>::range(1,5)
        .subscribe_on(observe_on_asio(io_service)); 
      }).max()
    )
  );
}


TEST(RxLazy, onError)
{
  boost::asio::io_service io_service;
  std::string error_message;

  pep::RxLazy<int>([&io_service](){ 
      return rxcpp::observable<>::error<int>(std::runtime_error("This error"))
        .subscribe_on(observe_on_asio(io_service)); 
  }).subscribe(
    [](int i){ ASSERT_TRUE(false); },
    [&error_message](std::exception_ptr ep) {
      try { std::rethrow_exception(ep); }
      catch (const std::exception& ex) {
        error_message = ex.what();
      }
    },
    [](){ ASSERT_TRUE(false); }
  );

  io_service.run();

  ASSERT_EQ(error_message, "This error");
}

TEST(RxLazy, ErrorInCreator)
{
  // An exception in the creator passed to RxLazy should result
  // in an on_error.
  boost::asio::io_service io_service;
  std::string error_message;

  pep::RxLazy<int>([]()->rxcpp::observable<int>{ 
    throw std::runtime_error("This error");
  }).subscribe_on(observe_on_asio(io_service)).subscribe(
    [](int i){ ASSERT_TRUE(false); },
    [&error_message](std::exception_ptr ep) {
      try { std::rethrow_exception(ep); }
      catch (const std::exception& ex) {
        error_message = ex.what();
      }
    },
    [](){ ASSERT_TRUE(false); }
  );

  io_service.run();

  ASSERT_EQ(error_message, "This error");
}

TEST(RxLazy, IsLazy)
{
  boost::asio::io_service io_service;
  bool True=true;

  ASSERT_EQ(std::vector<int>{5}, 
    *testutils::exhaust<int>(io_service,
      rxcpp::observable<>::range(1,5)
        .subscribe_on(observe_on_asio(io_service))
        .switch_if_empty(
      pep::RxLazy<int>([&True,&io_service]()->rxcpp::observable<int>{ 
        // Execution shouldn't reach this point
        True = false;
        return rxcpp::observable<>::error<int>(
            std::runtime_error("This shouldn't be"))
              .subscribe_on(observe_on_asio(io_service));
      })).max()));

  ASSERT_TRUE(True);
}

TEST(RxLazy, LazyPipeline)
{
  boost::asio::io_service io_service;
  bool True=true;

  ASSERT_EQ(std::vector<int>{5}, 
    *testutils::exhaust<int>(io_service,
      rxcpp::observable<>::range(1,5)
        .subscribe_on(observe_on_asio(io_service))
        .switch_if_empty(
      pep::RxLazy<int>([&True,&io_service]()->rxcpp::observable<int>{ 
        // Execution shouldn't reach this point
        True = false;
        return rxcpp::observable<>::error<int>(
            std::runtime_error("This shouldn't be"))
              .subscribe_on(observe_on_asio(io_service));
      }).map([](int in)->int{ return in+1; })).max()));

  ASSERT_TRUE(True);
}

}
