#include <gtest/gtest.h>
#include <pep/utils/LocalSettings.hpp>

namespace {

TEST(LocalSettingsTest, TestStoreAndRetrieveStringValue) {
  auto& settings = pep::LocalSettings::getInstance();
  std::string value;

  ASSERT_FALSE(settings->retrieveValue(&value, "namespace", "stringvalue"));

  ASSERT_TRUE(settings->storeValue("namespace", "stringvalue", "1337"));
  ASSERT_TRUE(settings->flushChanges());

  ASSERT_TRUE(settings->retrieveValue(&value, "namespace", "stringvalue"));
  ASSERT_EQ(value, "1337");

  ASSERT_TRUE(settings->deleteValue("namespace", "stringvalue"));
  ASSERT_TRUE(settings->flushChanges());
  ASSERT_FALSE(settings->retrieveValue(&value, "namespace", "stringvalue"));
}

TEST(LocalSettingsTest, TestStoreAndRetrieveIntValue) {
  auto& settings = pep::LocalSettings::getInstance();
  int value = 0;

  ASSERT_FALSE(settings->retrieveValue(&value, "namespace", "intvalue"));

  ASSERT_TRUE(settings->storeValue("namespace", "intvalue", 1337));
  ASSERT_TRUE(settings->flushChanges());

  ASSERT_TRUE(settings->retrieveValue(&value, "namespace", "intvalue"));
  ASSERT_EQ(value, 1337);

  ASSERT_TRUE(settings->deleteValue("namespace", "intvalue"));
  ASSERT_TRUE(settings->flushChanges());
  ASSERT_FALSE(settings->retrieveValue(&value, "namespace", "intvalue"));
}

TEST(LocalSettingsTest, TestStoreIntAndRetrieveString) {
  auto& settings = pep::LocalSettings::getInstance();

  settings->storeValue("namespace", "ambiguous", 1337);
  std::string retrieved;
  ASSERT_TRUE(settings->retrieveValue(&retrieved, "namespace", "ambiguous"));
  ASSERT_EQ(retrieved, "1337");

  ASSERT_TRUE(settings->deleteValue("namespace", "ambiguous"));
}

}
