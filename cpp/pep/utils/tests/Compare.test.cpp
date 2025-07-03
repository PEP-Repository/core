#include <gtest/gtest.h>

#include <pep/utils/Compare.hpp>

namespace {

TEST(CaseInsensitiveCompare, caseInsensitiveMapFind) {
  std::map<std::string, std::string, pep::CaseInsensitiveCompare> caseInsensitiveMap {
    {"TARGET", "value"},
  };

  auto result = caseInsensitiveMap.find("Target");
  ASSERT_TRUE(result != caseInsensitiveMap.end());

  ASSERT_EQ(caseInsensitiveMap.find("unexisting"), caseInsensitiveMap.end());
}

TEST(CaseInsensitiveCompare, caseInsensitiveMapFindUnexistingKey) {
  std::map<std::string, std::string, pep::CaseInsensitiveCompare> caseInsensitiveMap {
    {"TARGET", "value"},
  };

  ASSERT_EQ(caseInsensitiveMap.find("unexisting"), caseInsensitiveMap.end());
}

TEST(CaseInsensitiveCompare, caseInsensitiveMapOverwrite) {
  std::map<std::string, std::string, pep::CaseInsensitiveCompare> caseInsensitiveMap {
    {"TARGET", "value"},
  };
  caseInsensitiveMap["Target"] = "overwrite";

  auto result = caseInsensitiveMap.find("target");
  ASSERT_EQ(result->second, "overwrite");
  ASSERT_EQ(caseInsensitiveMap.size(), 1);
}

TEST(CaseInsensitiveCompare, caseInsensitiveMapEmplace) {
  std::map<std::string, std::string, pep::CaseInsensitiveCompare> caseInsensitiveMap {
    {"TARGET", "value"},
  };
  auto emplaced = caseInsensitiveMap.emplace("target", "noopt");

  auto result = caseInsensitiveMap.find("target");
  ASSERT_EQ(result->second, "value");
  ASSERT_EQ(emplaced.second, false);
  ASSERT_EQ(caseInsensitiveMap.size(), 1);
}

}
