#include <gtest/gtest.h>

#include <pep/accessmanager/AmaMessages.hpp>

namespace {

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, simple) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{1024};
  size_t offset{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, capactity, source, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  ASSERT_EQ(dest.name, expectedCG.name);
  ASSERT_EQ(dest.columns, expectedCG.columns);
  ASSERT_EQ(actualLength, 26U);
}

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, capacityZero) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{0};
  size_t offset{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, capactity, source, offset);

  //Assert
  ASSERT_EQ(dest.name, "");
  ASSERT_EQ(dest.columns, std::vector<std::string>{});
  ASSERT_EQ(actualLength, 0U);
}


TEST(AmaQRColumnGroupFillColumnGroupToCapacity, CapacityLimited) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{16};
  size_t offset{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, capactity, source, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col1"}};
  ASSERT_EQ(expectedCG.name, dest.name);
  ASSERT_EQ(expectedCG.columns, dest.columns);
  ASSERT_EQ(actualLength, 14U);
}

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, OffsetLimited) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{1024};
  size_t offset{2};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, capactity, source, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col3"}};
  ASSERT_EQ(expectedCG.name, dest.name);
  ASSERT_EQ(expectedCG.columns, dest.columns);
  ASSERT_EQ(actualLength, 14U);
}

TEST(AmaQRColumnGroupFillColumnGroupToCapacity, OffsetAndCapacity) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3", "col4"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{16};
  size_t offset{2};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, capactity, source, offset);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col3"}};
  ASSERT_EQ(expectedCG.name, dest.name);
  ASSERT_EQ(expectedCG.columns, dest.columns);
  ASSERT_EQ(actualLength, 14U);
}


TEST(AmaQRColumnGroupFillColumnGroupToCapacity, NoPadding) {
  // Arrange 
  pep::AmaQRColumnGroup source{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  pep::AmaQRColumnGroup dest{};
  size_t capactity{1024};
  size_t offset{0};
  size_t padding{0};

  //Act
  size_t actualLength = pep::AmaQRColumnGroup::FillToProtobufSerializationCapacity(dest, capactity, source, offset, padding);

  //Assert
  pep::AmaQRColumnGroup expectedCG{"cgName", std::vector<std::string>{"col1", "col2", "col3"}};
  ASSERT_EQ(dest.name, expectedCG.name);
  ASSERT_EQ(dest.columns, expectedCG.columns);
  ASSERT_EQ(actualLength, 18U);
}

}
