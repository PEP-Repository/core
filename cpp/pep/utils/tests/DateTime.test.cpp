#include <ctime>

#include <gtest/gtest.h>

//TODO Why are we testing the STL?
TEST(DateTime, CBased) {
  // Initialize a time_t (to the current timestamp)
  auto generated = std::time(nullptr);

  // Convert to a tm representing local time
  std::tm local = *std::localtime(&generated); //TODO not thread-safe

  // Verify that "localtime" function produces a known DST state
  ASSERT_GE(local.tm_isdst, 0);

  // Construct a tm equal to "local" but with unknown DST state
  std::tm constructed = local;
  constructed.tm_isdst = -1; // "less than zero if the information is not available": http://www.cplusplus.com/reference/ctime/tm/

  // Have mktime "parse" our constructed tm
  auto parsed = std::mktime(&constructed);

  // Verify that "mktime" updated the struct tm to a known DST state
  ASSERT_GE(local.tm_isdst, 0);

  // Verify that "mktime" parsed our "unknown" DST state to the correct (previously generated) timestamp
  ASSERT_EQ(generated, parsed);
}
