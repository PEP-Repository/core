#pragma once

#include <gtest/gtest.h>
#include <rxcpp/rx-lite.hpp>
#include <boost/asio/io_context.hpp>
#include <pep/async/RxTimeout.hpp>

namespace pep::testutils
{
  // Test an observable driven by the given io_context: runs (and resets)
  // the io_context, and collects the items emitted by the observable
  // in a vector, which is returned.  Checks that the observable ends
  // with precisely one on_error or on_complete.
  //
  // WARNING:  be sure the io_context is not already being run.
  template <class T>
  std::shared_ptr<std::vector<T>> exhaust(
      boost::asio::io_context& io_context,
      rxcpp::observable<T> obs)
  {
    bool shouldBeDone = false;
    std::exception_ptr storedEp;
    auto results = std::make_shared<std::vector<T>>();

    obs
      .op(RxAsioTimeout(std::chrono::seconds(1), io_context, observe_on_asio(io_context)))
      .subscribe(
      [&shouldBeDone, results](T item) {
        EXPECT_FALSE(shouldBeDone);
        results->push_back(item);
      },
      [&shouldBeDone, &storedEp](std::exception_ptr ep) {
        EXPECT_FALSE(shouldBeDone);
        shouldBeDone = true;
        storedEp = ep;
      },
      [&shouldBeDone](){
        EXPECT_FALSE(shouldBeDone);
        shouldBeDone = true;
      }
    );

    io_context.run();
    io_context.restart();

    EXPECT_TRUE(shouldBeDone);

    if (storedEp) {
      std::rethrow_exception(storedEp);
    }

    return results;
  }
}

