#include <gtest/gtest.h>

#include <pep/pullcastor/PullCastorUtils.hpp>

#include <boost/property_tree/ptree.hpp>

using namespace std::literals;
using namespace pep;

namespace {

TEST(PullCastorUtils, ParseCastorDateTime_Amsterdam) {
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-11-19 13:49:44.000000");
  datetimeObject.put("timezone", "Europe/Amsterdam");
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1605790184s});
}

TEST(PullCastorUtils, ParseCastorDateTime_UTC) {
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-11-19 13:49:44.000000");
  datetimeObject.put("timezone", "Etc/UTC");
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1605793784s});
}

TEST(PullCastorUtils, ParseCastorDateTime_Plus5) {
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-11-19 13:49:44.000000");
  datetimeObject.put("timezone", "Indian/Maldives"); // UTC+5 a.k.a. Etc/GMT-5
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1605775784s});
}

}
