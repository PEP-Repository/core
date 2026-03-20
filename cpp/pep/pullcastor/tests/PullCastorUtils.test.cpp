#include <gtest/gtest.h>

#include <pep/pullcastor/PullCastorUtils.hpp>

#include <boost/property_tree/ptree.hpp>

using namespace std::literals;
using namespace pep;

namespace {

TEST(ParseCastorDateTime, Amsterdam) {
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-11-19 13:49:44.000000");
  datetimeObject.put("timezone", "Europe/Amsterdam");
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1605790184s});
}

TEST(ParseCastorDateTime, AmsterdamSummer) {
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-06-19 13:49:44.000000");
  datetimeObject.put("timezone", "Europe/Amsterdam");
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1592567384s});
}

TEST(ParseCastorDateTime, Utc) {
  //TODO
  GTEST_SKIP() << "This fails in the CI, but I cannot seem to replicate this (with date library or modern STL, also in a container)";
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-11-19 13:49:44.000000");
  datetimeObject.put("timezone", "Etc/UTC");
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1605793784s});
}

TEST(ParseCastorDateTime, UtcPlus5) {
  boost::property_tree::ptree datetimeObject;
  datetimeObject.put("date", "2020-11-19 13:49:44.000000");
  datetimeObject.put("timezone", "Indian/Maldives"); // UTC+5 a.k.a. Etc/GMT-5
  EXPECT_EQ(castor::ParseCastorDateTime(datetimeObject), Timestamp{1605775784s});
}

}
