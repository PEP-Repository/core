#include <gtest/gtest.h>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/accessmanager/AccessManager.hpp>

using namespace pep;

namespace {

TEST(AccessManagerTest, extractPartialColumnGroupQueryResponse_simple) {
  // Arrange
  std::vector <AmaQRColumnGroup> input{
    {"cg1", std::vector<std::string>{"col1", " col2", "col3", "col4", "col5", "col6"}},
    {"cg2", std::vector<std::string>{"col7", " col8", "col9", "colA", "colB", "colC"}},
    {"cg3", std::vector<std::string>{"colD", " colE", "colF", "colG", "colH", "colI"}}};

  auto actualColumnGroups = std::vector<AmaQRColumnGroup>();

  // Act
  auto responses = AccessManager::ExtractPartialColumnGroupQueryResponse(input);
  for (const auto& response : responses) {
    for (const auto& entry : response.mColumnGroups) {
      actualColumnGroups.push_back(entry);
    }
  }
  // Assert
  ASSERT_EQ(responses.size(), 1U);
  ASSERT_EQ(actualColumnGroups.size(), 3);
}

TEST(AccessManagerTest, extractPartialColumnGroupQueryResponse_smallMessageSize) {
  // Arrange
  std::vector <AmaQRColumnGroup> input{{"cg1", std::vector<std::string>{"col1", "col2", "col3"}}};


  auto actualColumnGroups = std::vector<AmaQRColumnGroup>();

  // Act
  auto responses = AccessManager::ExtractPartialColumnGroupQueryResponse(input, 15U);
  for (const auto& response : responses) {
    for (const auto& entry : response.mColumnGroups) {
      actualColumnGroups.push_back(entry);
    }
  }
  // Assert
  ASSERT_EQ(responses.size(), 3U);
  ASSERT_EQ(actualColumnGroups.size(), 3);
}


TEST(AccessManagerTest, extractPartialColumnGroupQueryResponse_EmptyColumnGroup) {
  std::vector <AmaQRColumnGroup> input{
    {"cg1", std::vector<std::string>{"col1", " col2", "col3", "col4", "col5", "col6"}},
    {"cg2", std::vector<std::string>{"col7", " col8", "col9", "colA", "colB", "colC"}},
    {"cg3", std::vector<std::string>{"colD", " colE", "colF", "colG", "colH", "colI"}},
    {"cgName4", std::vector<std::string>{}},};

  auto actualColumnGroups = std::vector<AmaQRColumnGroup>();

  // Act
  auto responses = AccessManager::ExtractPartialColumnGroupQueryResponse(input);
  for (const auto& response : responses) {
    for (const auto& entry : response.mColumnGroups) {
      actualColumnGroups.push_back(entry);
    }
  }
  // Assert
  ASSERT_EQ(responses.size(), 1U);
  ASSERT_EQ(actualColumnGroups.size(), 4);
}

TEST(AccessManagerTest, extractPartialColumnGroupQueryResponse_CatchInfinteLoop) {
  // Arrange
  std::vector <AmaQRColumnGroup> input{{"A_Very_Long_Name_Of_The_ColumnGroup", std::vector<std::string>{"col1", "col2", "col3"}}};
  auto actualColumnGroups = std::vector<AmaQRColumnGroup>();

  try {
    auto actualColumnGroups = std::vector<AmaQRColumnGroup>();

    // Act
    auto responses = AccessManager::ExtractPartialColumnGroupQueryResponse(input, 15U);

    FAIL() << "This should not have run without exceptions.";
  }
  catch (const std::runtime_error& e) {
    // Assert
    std::string expectedMessage = "Processing column group A_Very_Long_Name_Of_The_ColumnGroup, a new AmaQueryResponse was prompted while the last response was still empty. Is the maxSize set correctly? maxSize: 15";
    ASSERT_EQ(e.what(), expectedMessage);
  }
}

}
