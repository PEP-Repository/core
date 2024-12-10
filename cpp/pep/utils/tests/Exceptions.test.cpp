#include <pep/utils/Exceptions.hpp>

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>

TEST(Exceptions, GetExceptionMessage) {
  auto boost = std::make_exception_ptr(boost::asio::invalid_service_owner());
  EXPECT_TRUE(boost::contains(pep::GetExceptionMessage(boost), "invalid_service_owner")); // (Run time) type name should be included
  EXPECT_TRUE(boost::contains(pep::GetExceptionMessage(boost), "service owner")); // Exception's what() should be included

  auto std = std::make_exception_ptr(std::runtime_error("They speak English in What?"));
  EXPECT_TRUE(boost::contains(pep::GetExceptionMessage(std), "runtime_error")); // (Run time) type name should be included
  EXPECT_TRUE(boost::contains(pep::GetExceptionMessage(std), "English")); // Exception's what() should be included

  auto nonstd = std::make_exception_ptr(42);
  EXPECT_TRUE(boost::contains(pep::GetExceptionMessage(nonstd), "int") // Type name should be included if the platform supports it, or...
    || pep::GetExceptionMessage(nonstd) == "[unknown exception type]"); // ... our code should produce this placeholder

  EXPECT_EQ(pep::GetExceptionMessage(nullptr), "[null std::exception_ptr]"); // nullptr exception pointers should produce this placeholder
}
